// Copyright (C) 2025 ...

#include "persistence_writer.hpp"

#include <cstdint>
#include <map>
#include <memory>
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
    storage_containers.insert(std::make_pair(std::make_unique<placement>(plcmnt), type));
  }
  store_writer_->create_containers(storage_containers, tech_settings_);
}

token persistence_writer::register_write(placement const& plcmnt,
                                         void const* data,
                                         std::type_info const& type)
{
  std::uint64_t const row = store_writer_->fill_container(plcmnt, data, type);
  // A returned token must locate a readable product: its row is the read-side navigation key.
  // invalid_row_id means the backend does not address rows, so a product routed there could not be
  // located on read; reject it here rather than return an unusable token.
  if (row == invalid_row_id) {
    throw std::runtime_error("persistence_writer::register_write backend for container '" +
                             plcmnt.container_name() +
                             "' does not address rows; cannot produce a token locating the "
                             "written product");
  }
  return token{plcmnt.file_name(), plcmnt.container_name(), plcmnt.technology(), row};
}

void persistence_writer::fill_index(placement const& index_place, std::string const& id)
{
  store_writer_->fill_container(index_place, &id, typeid(std::string));
}

void persistence_writer::commit_place(placement const& plcmnt)
{
  store_writer_->commit_containers(plcmnt);
}
