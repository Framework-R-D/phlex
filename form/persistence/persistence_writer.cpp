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

namespace {
  // The navigation ("index") container lives alongside its product.
  placement index_placement_for(placement const& product_place)
  {
    std::string const& name = product_place.container_name();
    auto const slash = name.find('/');
    std::string const creator = slash == std::string::npos ? name : name.substr(0, slash);
    return placement{
      product_place.file_name(), build_full_label(creator, "index"), product_place.technology()};
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

void persistence_writer::commit_place(placement const& plcmnt, std::string const& id)
{
  auto const it = index_by_product_.find(
    std::make_tuple(plcmnt.file_name(), plcmnt.container_name(), plcmnt.technology()));
  placement const index_place =
    it != index_by_product_.end() ? it->second : index_placement_for(plcmnt);
  store_writer_->fill_container(index_place, &id, typeid(std::string));
  store_writer_->commit_containers(plcmnt);
}
