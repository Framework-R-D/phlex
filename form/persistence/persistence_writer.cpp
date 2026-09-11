// Copyright (C) 2025 ...

#include "persistence_writer.hpp"

#include "core/cell_index.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

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
    auto const it = index_by_product_.find(
      std::make_tuple(plcmnt.file_name(), plcmnt.container_name(), plcmnt.technology()));
    placement const index_place =
      it != index_by_product_.end() ? it->second : index_placement_for(plcmnt);
    store_writer_->fill_container(index_place, &cell.id, typeid(std::string));
    store_writer_->commit_containers(plcmnt);

    // Record navigation only after the product commit succeeds.
    record_navigation(plcmnt, cell);
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

persistence_writer::navigation_table& persistence_writer::table_for(navigation_key const& key,
                                                                    cell_index const& cell)
{
  auto& table = navigation_tables_[key];
  if (!table.layers_set) {
    table.layer_names = cell.layer_names;
    table.layers_set = true;
    return table;
  }
  if (table.layer_names == cell.layer_names) {
    return table;
  }

  std::string existing;
  for (auto const& name : table.layer_names) {
    if (!existing.empty()) {
      existing += ", ";
    }
    existing += name;
  }
  throw std::runtime_error("persistence_writer: data cell " + cell.id + " maps to hierarchy '" +
                           key.hierarchy_key + "', which is already indexed with layer names [" +
                           existing + "]");
}

void persistence_writer::record_dictionary_entries(place_key const& place,
                                                   std::vector<pending_write> const& pending,
                                                   std::string const& hierarchy,
                                                   technology::id tech)
{
  auto& dictionary = dictionaries_[place];
  for (auto const& write : pending) {
    dictionary.try_emplace(
      std::make_tuple(write.creator, write.label, hierarchy),
      dictionary_entry{.product_name = write.label,
                       .creator = write.creator,
                       .container_name = write.container_name,
                       .hierarchy_key = hierarchy,
                       .navigation_container = navigation_table_name(hierarchy, tech),
                       .navigation_column = navigation_row_column(write.creator)});
  }
}

void persistence_writer::record_navigation(placement const& plcmnt, cell_index const& cell)
{
  // Remove pending writes before processing the record.
  place_key const place{plcmnt.file_name(), plcmnt.technology()};
  auto const pending = std::exchange(pending_by_place_[place], {});
  if (pending.empty()) {
    return;
  }

  auto const row_by_creator = rows_by_creator(pending, cell);

  if (!cell.consistent()) {
    throw std::runtime_error("persistence_writer: data cell " + cell.id + " has " +
                             std::to_string(cell.layer_names.size()) + " layer names but " +
                             std::to_string(cell.layer_values.size()) + " layer values");
  }

  auto const hierarchy = hierarchy_key(cell.layer_names);
  auto& table = table_for(navigation_key{.file_name = plcmnt.file_name(),
                                         .technology = plcmnt.technology(),
                                         .hierarchy_key = hierarchy},
                          cell);

  auto& cell_rows = table.rows[cell.layer_values];
  for (auto const& [creator, row] : row_by_creator) {
    auto const [it, inserted] = cell_rows.try_emplace(creator, row);
    if (!inserted) {
      // One navigation row per creator and data cell.
      throw std::runtime_error("persistence_writer: creator '" + creator + "' wrote data cell " +
                               cell.id +
                               " more than once; the navigation table holds one row per creator "
                               "per data cell");
    }
    table.creators.insert(creator);
  }

  record_dictionary_entries(place, pending, hierarchy, plcmnt.technology());
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

void persistence_writer::write_navigation_tables()
{
  for (auto const& [key, table] : navigation_tables_) {
    std::vector<std::string> columns;
    columns.reserve(table.layer_names.size() + table.creators.size());
    for (auto const& layer_name : table.layer_names) {
      columns.push_back(sanitize_name(layer_name));
    }
    for (auto const& creator : table.creators) {
      columns.push_back(navigation_row_column(creator));
    }

    std::set<std::string> seen;
    for (auto const& column : columns) {
      if (!seen.insert(column).second) {
        throw std::runtime_error("persistence_writer: navigation table for hierarchy '" +
                                 key.hierarchy_key + "' has two columns named '" + column + "'");
      }
    }

    auto const table_name = navigation_table_name(key.hierarchy_key, key.technology);
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
