// Copyright (C) 2025 ...

#include "persistence_writer.hpp"

#include "core/cell_index.hpp"
#include "core/container_naming.hpp"
#include "core/placement.hpp"
#include "core/technology.hpp"
#include "core/token.hpp"
#include "navigation_naming.hpp"
#include "persistence/ipersistence_writer.hpp"
#include "storage/istorage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <vector>

using namespace form::detail::experimental;

namespace {
  // Extract the creator from a "creator/label" container name.
  std::string creator_of(std::string const& container_name)
  {
    auto const slash = container_name.find('/');
    return slash == std::string::npos ? container_name : container_name.substr(0, slash);
  }

  std::string label_of(std::string const& container_name)
  {
    auto const slash = container_name.find('/');
    return slash == std::string::npos ? std::string{} : container_name.substr(slash + 1);
  }

  // The navigation ("index") container lives alongside its product.
  placement index_placement_for(placement const& product_place)
  {
    return placement{product_place.file_name(),
                     build_full_label(creator_of(product_place.container_name()), "index"),
                     product_place.technology()};
  }

  std::string layer_names_text(cell_hierarchy const& hierarchy)
  {
    std::string text;
    for (auto const& name : hierarchy.layer_names) {
      if (!text.empty()) {
        text += ", ";
      }
      // Show unnamed layers rather than leaving a gap the reader has to count.
      text += name.empty() ? "<unnamed>" : name;
    }
    return text;
  }

  std::string layer_label(std::string const& layer_name, std::size_t position)
  {
    return layer_name.empty() ? "the unnamed layer at position " + std::to_string(position)
                              : "layer '" + layer_name + "'";
  }
}

namespace form::detail::experimental {
  std::unique_ptr<i_persistence_writer> create_persistence_writer()
  {
    return std::make_unique<persistence_writer>();
  }
}

persistence_writer::persistence_writer() : store_writer_(create_storage_writer()), tech_settings_()
{
}

persistence_writer::persistence_writer(std::unique_ptr<i_storage_writer> store_writer) :
  store_writer_(std::move(store_writer)), tech_settings_()
{
}

void persistence_writer::configure_tech_settings(
  form::experimental::config::tech_setting_config const& tech_config_settings)
{
  tech_settings_ = tech_config_settings;
}

void persistence_writer::create_containers(
  std::vector<std::pair<placement, std::type_info const*>> const& containers)
{
  std::map<std::unique_ptr<placement>, std::type_info const*> storage_containers;
  for (auto const& [plcmnt, type] : containers) {
    // Reserve the "navigation_prefix" namespace for navigation containers.
    if (creator_of(plcmnt.container_name()).starts_with(navigation_prefix)) {
      throw std::runtime_error("persistence_writer::create_containers creator name '" +
                               creator_of(plcmnt.container_name()) + "' begins with '" +
                               std::string{navigation_prefix} +
                               "', which is reserved for FORM navigation containers");
    }

    storage_containers.insert(std::make_pair(std::make_unique<placement>(plcmnt), type));

    // Persistence owns navigation: every product container gets an index container alongside it.
    placement index_place = index_placement_for(plcmnt);
    auto const [it, inserted] = index_by_product_.try_emplace(
      std::make_tuple(plcmnt.file_name(), plcmnt.container_name(), plcmnt.technology()),
      index_place);
    if (inserted) {
      storage_containers.insert(
        std::make_pair(std::make_unique<placement>(std::move(index_place)), &typeid(std::string)));
    }
  }
  store_writer_->create_containers(storage_containers, tech_settings_);
}

token persistence_writer::register_write(placement const& plcmnt,
                                         void const* data,
                                         std::type_info const& type)
{
  place_key const place{plcmnt.file_name(), plcmnt.technology()};

  // Discard all pending writes if this record fails.
  auto const abandon_record = [this] { pending_by_place_.clear(); };

  std::uint64_t row = invalid_row_id;
  try {
    row = store_writer_->fill_container(plcmnt, data, type);
  } catch (...) {
    abandon_record();
    throw;
  }

  // A returned token must locate a readable product: its row is the read-side navigation key.
  // invalid_row_id means the backend does not address rows, so a product routed there could not be
  // located on read; reject it here rather than return an unusable token.
  if (row == invalid_row_id) {
    abandon_record();
    throw std::runtime_error("persistence_writer::register_write backend for container '" +
                             plcmnt.container_name() +
                             "' does not address rows; cannot produce a token locating the "
                             "written product");
  }

  // Remember the write for navigation table; commit_place() supplies the data cell that keys it.
  pending_by_place_[place].push_back(pending_write{.creator = creator_of(plcmnt.container_name()),
                                                   .label = label_of(plcmnt.container_name()),
                                                   .container_name = plcmnt.container_name(),
                                                   .row = row});

  return token{plcmnt.file_name(), plcmnt.container_name(), plcmnt.technology(), row};
}

void persistence_writer::commit_place(placement const& plcmnt, cell_index const& cell)
{
  try {
    auto staged = stage_navigation(plcmnt, cell);

    auto const it = index_by_product_.find(
      std::make_tuple(plcmnt.file_name(), plcmnt.container_name(), plcmnt.technology()));
    placement const index_place =
      it != index_by_product_.end() ? it->second : index_placement_for(plcmnt);
    store_writer_->fill_container(index_place, &cell.id, typeid(std::string));
    store_writer_->commit_containers(plcmnt);

    // The record is on disk; now record where it went.
    apply_navigation(std::move(staged));
  } catch (...) {
    pending_by_place_.clear();
    throw;
  }
}

std::map<std::string, std::uint64_t> persistence_writer::rows_by_creator(
  std::vector<pending_write> const& pending, cell_index const& cell)
{
  // A creator must use one row for all products written for a data cell.
  std::map<std::string, std::uint64_t> row_by_creator;
  for (auto const& write : pending) {
    auto const [it, inserted] = row_by_creator.try_emplace(write.creator, write.row);
    if (!inserted && it->second != write.row) {
      throw std::runtime_error(
        "persistence_writer: creator '" + write.creator + "' wrote container '" +
        write.container_name + "' at row " + std::to_string(write.row) +
        " but its other products for data cell " + cell.id + " went to row " +
        std::to_string(it->second) +
        "; all products a creator writes for one data cell must share a row for navigation to "
        "locate them");
    }
  }
  return row_by_creator;
}

void persistence_writer::record_dictionary_entries(place_key const& place,
                                                   std::vector<pending_write> const& pending,
                                                   cell_hierarchy const& hierarchy,
                                                   technology::id tech)
{
  auto& dictionary = dictionaries_[place];
  for (auto const& write : pending) {
    dictionary.try_emplace(
      std::make_tuple(write.creator, write.label, hierarchy),
      dictionary_entry{.product_name = write.label,
                       .creator = write.creator,
                       .container_name = write.container_name,
                       .hierarchy_key = hierarchy_key(hierarchy),
                       .navigation_container = navigation_table_name(hierarchy, tech),
                       .navigation_column = navigation_row_column(write.creator)});
  }
}

persistence_writer::staged_record persistence_writer::stage_navigation(placement const& plcmnt,
                                                                       cell_index const& cell)
{
  // Remove pending writes before processing the record.
  place_key place{plcmnt.file_name(), plcmnt.technology()};
  auto pending = std::exchange(pending_by_place_[place], {});

  if (!cell.consistent()) {
    throw std::runtime_error("persistence_writer: data cell " + cell.id + " has " +
                             std::to_string(cell.hierarchy.layer_names.size()) +
                             " layer names but " + std::to_string(cell.layer_values.size()) +
                             " layer values");
  }

  if (pending.empty()) {
    return staged_record{};
  }

  auto row_by_creator = rows_by_creator(pending, cell);
  auto const& hierarchy = cell.hierarchy;
  navigation_key key{
    .file_name = plcmnt.file_name(), .technology = plcmnt.technology(), .hierarchy = hierarchy};
  auto table_name = navigation_table_name(hierarchy, key.technology);

  auto const table_it = navigation_tables_.find(key);
  auto const* table = table_it != navigation_tables_.end() ? &table_it->second : nullptr;

  auto const claims_table_name = check_table_name(key, table_name);
  check_rows_are_new(table, cell, row_by_creator);

  std::map<std::string, std::string> new_columns;
  if (table == nullptr) {
    for (std::size_t position = 0; position < hierarchy.layer_names.size(); ++position) {
      auto const& layer_name = hierarchy.layer_names[position];
      stage_column(new_columns,
                   table,
                   table_name,
                   layer_column_name(layer_name, position),
                   layer_label(layer_name, position));
    }
  }
  for (auto const& [creator, row] : row_by_creator) {
    if (table != nullptr && table->creators.contains(creator)) {
      continue;
    }
    stage_column(
      new_columns, table, table_name, navigation_row_column(creator), "creator '" + creator + "'");
  }

  return staged_record{.place = std::move(place),
                       .key = std::move(key),
                       .table_name = std::move(table_name),
                       .pending = std::move(pending),
                       .row_by_creator = std::move(row_by_creator),
                       .layer_values = cell.layer_values,
                       .new_columns = std::move(new_columns),
                       .claims_table_name = claims_table_name};
}

void persistence_writer::apply_navigation(staged_record staged)
{
  if (staged.pending.empty()) {
    return;
  }

  if (staged.claims_table_name) {
    claimed_table_names_.emplace(
      std::make_tuple(staged.key.file_name, staged.key.technology, std::move(staged.table_name)),
      staged.key.hierarchy);
  }

  // The key is still needed below, so only the record's own storage is taken here.
  auto& table = navigation_tables_[staged.key];
  // merge() splices the staged nodes across rather than copying them.
  table.column_sources.merge(staged.new_columns);

  auto& cell_rows = table.rows[std::move(staged.layer_values)];
  for (auto const& [creator, row] : staged.row_by_creator) {
    cell_rows.emplace(creator, row);
    table.creators.insert(creator);
  }

  record_dictionary_entries(
    staged.place, staged.pending, staged.key.hierarchy, staged.key.technology);
}

void persistence_writer::finalize()
{
  if (finalized_) {
    return;
  }
  finalized_ = true;

  write_navigation_tables();
  write_product_dictionaries();
}

bool persistence_writer::check_table_name(navigation_key const& key,
                                          std::string const& table_name) const
{
  auto const it =
    claimed_table_names_.find(std::make_tuple(key.file_name, key.technology, table_name));
  if (it == claimed_table_names_.end()) {
    return true;
  }
  if (it->second != key.hierarchy) {
    throw std::runtime_error(
      "persistence_writer: hierarchies [" + layer_names_text(it->second) + "] and [" +
      layer_names_text(key.hierarchy) + "] both name their navigation container '" + table_name +
      "' in file '" + key.file_name + "'; rename a layer so the two can be told apart on disk");
  }
  return false;
}

void persistence_writer::check_rows_are_new(
  navigation_table const* table,
  cell_index const& cell,
  std::map<std::string, std::uint64_t> const& row_by_creator)
{
  if (table == nullptr) {
    return;
  }
  auto const rows_it = table->rows.find(cell.layer_values);
  if (rows_it == table->rows.end()) {
    return;
  }
  for (auto const& [creator, row] : row_by_creator) {
    if (rows_it->second.contains(creator)) {
      throw std::runtime_error("persistence_writer: creator '" + creator + "' wrote data cell " +
                               cell.id +
                               " more than once; the navigation table holds one row per creator "
                               "per data cell");
    }
  }
}

void persistence_writer::stage_column(std::map<std::string, std::string>& staged,
                                      navigation_table const* table,
                                      std::string const& table_name,
                                      std::string const& column,
                                      std::string const& source)
{
  auto const clash = [&](std::string const& holder) {
    return std::runtime_error("persistence_writer: in navigation table '" + table_name + "', " +
                              holder + " and " + source + " both name column '" + column +
                              "'; rename one so the two can be told apart on disk");
  };

  if (table != nullptr) {
    auto const it = table->column_sources.find(column);
    if (it != table->column_sources.end()) {
      throw clash(it->second);
    }
  }
  auto const [it, inserted] = staged.try_emplace(column, source);
  if (!inserted) {
    throw clash(it->second);
  }
}

void persistence_writer::write_navigation_tables()
{
  for (auto const& [key, table] : navigation_tables_) {
    auto const table_name = navigation_table_name(key.hierarchy, key.technology);

    std::vector<std::string> columns;
    columns.reserve(key.hierarchy.layer_names.size() + table.creators.size());
    for (std::size_t position = 0; position < key.hierarchy.layer_names.size(); ++position) {
      columns.push_back(layer_column_name(key.hierarchy.layer_names[position], position));
    }
    for (auto const& creator : table.creators) {
      columns.push_back(navigation_row_column(creator));
    }

    auto const places = create_table_columns(
      key.file_name, key.technology, table_name, columns, typeid(std::uint64_t));

    // Keep bound values alive until the row is committed.
    std::vector<std::uint64_t> row_values(places.size());

    for (auto const& [layer_values, rows_by_creator] : table.rows) {
      std::size_t column = 0;
      for (auto const layer_value : layer_values) {
        row_values.at(column++) = layer_value;
      }
      for (auto const& creator : table.creators) {
        auto const it = rows_by_creator.find(creator);
        // Missing creator rows are represented by invalid_row_id.
        row_values.at(column++) = it != rows_by_creator.end() ? it->second : invalid_row_id;
      }

      for (std::size_t i = 0; i < places.size(); ++i) {
        store_writer_->fill_container(places[i], &row_values[i], typeid(std::uint64_t));
      }
      // Every column of the row is filled, so committing any one of them advances the table.
      store_writer_->commit_containers(places.front());
    }
  }
}

void persistence_writer::write_product_dictionaries()
{
  // Technology is encoded in the dictionary container name.
  static constexpr std::array<std::string_view, 6> column_names{"product_name",
                                                                "creator",
                                                                "container_name",
                                                                "hierarchy_key",
                                                                "navigation_container",
                                                                "navigation_column"};

  for (auto const& [place, entries] : dictionaries_) {
    if (entries.empty()) {
      continue;
    }
    auto const& [file_name, tech] = place;

    std::vector<std::string> columns;
    columns.reserve(column_names.size());
    for (auto const column : column_names) {
      columns.emplace_back(column);
    }

    auto const places = create_table_columns(
      file_name, tech, navigation_dictionary_name(tech), columns, typeid(std::string));

    for (auto const& [dict_key, entry] : entries) {
      std::array<std::string const*, column_names.size()> const values{&entry.product_name,
                                                                       &entry.creator,
                                                                       &entry.container_name,
                                                                       &entry.hierarchy_key,
                                                                       &entry.navigation_container,
                                                                       &entry.navigation_column};
      for (std::size_t column = 0; column < values.size(); ++column) {
        store_writer_->fill_container(places.at(column), values.at(column), typeid(std::string));
      }
      store_writer_->commit_containers(places.front());
    }
  }
}

std::vector<placement> persistence_writer::create_table_columns(
  std::string const& file_name,
  form::technology::id tech,
  std::string const& table_name,
  std::vector<std::string> const& columns,
  std::type_info const& type)
{
  std::vector<placement> places;
  places.reserve(columns.size());
  for (auto const& column : columns) {
    placement place{file_name, build_full_label(table_name, column), tech};
    // Create one column at a time so create_containers preserves the input column order;
    // its map is keyed by unique_ptr, so batching columns would order them by pointer value.
    std::map<std::unique_ptr<placement>, std::type_info const*> one_column;
    one_column.emplace(std::make_unique<placement>(place), &type);
    store_writer_->create_containers(one_column, tech_settings_);
    places.push_back(std::move(place));
  }
  return places;
}
