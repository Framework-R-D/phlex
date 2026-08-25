// Copyright (C) 2025 ...

#include "persistence_writer.hpp"
#include "persistence_utils.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

using namespace form::detail::experimental;

namespace form::detail::experimental {
  std::unique_ptr<i_persistence_writer> create_persistence_writer()
  {
    return std::make_unique<persistence_writer>();
  }
}

persistence_writer::persistence_writer() :
  store_writer_(create_storage_writer()),
  config_items_(),
  tech_settings_() // constructor takes form config
{
}

void persistence_writer::configure_tech_settings(
  form::experimental::config::tech_setting_config const& tech_config_settings)
{
  tech_settings_ = tech_config_settings;
}

void persistence_writer::configure(form::experimental::config::item_config const& config_items)
{
  config_items_ = config_items;
}

void persistence_writer::create_containers(
  std::string const& creator, std::map<std::string, std::type_info const*> const& products)
{
  std::map<std::unique_ptr<placement>, std::type_info const*> containers;
  for (auto const& [label, type] : products) {
    containers.insert(std::make_pair(get_placement(creator, label), type));
  }
  containers.insert(std::make_pair(get_placement(creator, "index"), &typeid(std::string)));
  store_writer_->create_containers(containers, tech_settings_);
}

token persistence_writer::register_write(std::string const& creator,
                                         std::string const& label,
                                         void const* data,
                                         std::type_info const& type)
{
  std::unique_ptr<placement> plcmnt = get_placement(creator, label);
  std::uint64_t const row = store_writer_->fill_container(*plcmnt, data, type);
  // A returned token must locate a readable product: its row is the read-side navigation key.
  // invalid_row_id means backend does not address rows,so a product routed there could not be located on read, so throw here for such an unusable token
  if (row == invalid_row_id) {
    throw std::runtime_error("persistence_writer::register_write backend for product '" + label +
                             "' from creator '" + creator + "' does not address rows; " +
                             "cannot produce a token locating the written product");
  }
  return token{plcmnt->file_name(), plcmnt->container_name(), plcmnt->technology(), row};
}

void persistence_writer::commit_output(std::string const& creator, std::string const& id)
{
  std::unique_ptr<placement> plcmnt = get_placement(creator, "index");
  store_writer_->fill_container(*plcmnt, &id, typeid(std::string));
  store_writer_->commit_containers(*plcmnt);
}

std::unique_ptr<placement> persistence_writer::get_placement(std::string const& creator,
                                                             std::string const& label)
{
  auto const config_item = find_config_item(config_items_, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = build_full_label(creator, label);
  return std::make_unique<placement>(config_item->file_name, full_label, config_item->technology);
}
