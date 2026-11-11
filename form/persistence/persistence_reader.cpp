// Copyright (C) 2025 ...

#include "persistence_reader.hpp"

#include "core/container_naming.hpp"
#include "core/token.hpp"
#include "descriptor.hpp"
#include "persistence/ipersistence_reader.hpp"
#include "persistence_utils.hpp"
#include "storage/istorage.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

using namespace form::detail::experimental;

namespace {
  // ROOT is currently the only implemented major technology for disk files.
  // TODO: When another on-disk format is implemented, use magic-byte detection to identify the file
  // format rather than assuming ROOT here.
  constexpr auto bootstrap_file_format = form::technology::major::root;
}

namespace form::detail::experimental {
  std::unique_ptr<i_persistence_reader> create_persistence_reader()
  {
    return std::make_unique<persistence_reader>();
  }
}

persistence_reader::persistence_reader() : store_reader_(create_storage_reader()) {}

void persistence_reader::configure_tech_settings(
  form::experimental::config::tech_setting_config const& tech_config_settings)
{
  tech_settings_ = tech_config_settings;
}

void persistence_reader::configure(form::experimental::config::item_config const& config_items)
{
  config_items_ = config_items;
}

void persistence_reader::read(std::string const& creator,
                              std::string const& stage,
                              std::string const& label,
                              std::string const& id,
                              void const** data,
                              std::type_info const& type)
{
  std::unique_ptr<token> const token = get_token(creator, stage, label, id);
  store_reader_->read_container(*token, data, type, tech_settings_);
}

void persistence_reader::prime(std::string const& creator,
                               std::string const& stage,
                               std::string const& label,
                               std::type_info const& type)
{
  auto const config_item = find_config_item(config_items_, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }
  auto const file_technology = technology_for_product(config_item->file_name, label);

  std::string const full_label =
    build_full_label(build_row_space_name(file_technology, creator, stage), label);
  store_reader_->prime(
    token{config_item->file_name, full_label, file_technology}, type, tech_settings_);
}

std::vector<std::string> persistence_reader::list_indices(std::string const& creator,
                                                          std::string const& stage,
                                                          std::string const& label)
{
  auto const config_item = find_config_item(config_items_, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }
  auto const file_technology = technology_for_product(config_item->file_name, label);

  std::string const full_label =
    build_full_label(build_row_space_name(file_technology, creator, stage), "index");
  return store_reader_->list_indices(token{config_item->file_name, full_label, file_technology},
                                     tech_settings_);
}

std::unique_ptr<token> persistence_reader::get_token(std::string const& creator,
                                                     std::string const& stage,
                                                     std::string const& label,
                                                     std::string const& id)
{
  auto const config_item = find_config_item(config_items_, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }
  auto const file_technology = technology_for_product(config_item->file_name, label);

  std::string const row_space = build_row_space_name(file_technology, creator, stage);
  std::string const full_label = build_full_label(row_space, label);
  std::string const index_label = build_full_label(row_space, "index");

  std::uint64_t const row_id = store_reader_->get_index(
    token{config_item->file_name, index_label, file_technology}, id, tech_settings_);
  return std::make_unique<token>(config_item->file_name, full_label, file_technology, row_id);
}

std::vector<form::technology::id> const& persistence_reader::technologies_of(
  std::string const& file_name)
{
  if (auto const known = file_technologies_.find(file_name); known != file_technologies_.end()) {
    return known->second;
  }

  auto const entries = store_reader_->read_descriptor(file_name, bootstrap_file_format);
  if (!entries) {
    throw std::runtime_error("FORM: file '" + file_name +
                             "' carries no layout descriptor, so FORM cannot tell which layout "
                             "or which storage technologies it holds");
  }

  auto const version = descriptor_entry(*entries, descriptor_layout_version_key);
  if (!version) {
    throw std::runtime_error("FORM: the layout descriptor in file '" + file_name +
                             "' does not name a layout version");
  }
  check_layout_version(*version, file_name);

  auto const technologies_text = descriptor_entry(*entries, descriptor_technologies_key);
  if (!technologies_text) {
    throw std::runtime_error("FORM: the layout descriptor in file '" + file_name +
                             "' does not name a storage technology");
  }

  return file_technologies_.emplace(file_name, parse_technologies(*technologies_text, file_name))
    .first->second;
}

form::technology::id persistence_reader::technology_for_product(std::string const& file_name,
                                                                std::string const& label)
{
  auto const& technologies = technologies_of(file_name);
  if (technologies.size() == 1) {
    return technologies.front();
  }

  // Product-level technology selection requires the product dictionary.
  std::string names;
  for (auto const& technology : technologies) {
    if (!names.empty()) {
      names += ", ";
    }
    names += form::technology::on_disk_token(technology);
  }

  // TODO: Resolve the product's storage technology through read-aware navigation.
  throw std::runtime_error(
    "FORM: file '" + file_name + "' contains multiple storage technologies (" + names +
    "); cannot determine the storage technology for product '" + label + "'");
}
