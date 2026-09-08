// Copyright (C) 2025 ...

#include "form_writer.hpp"

#include <iostream>
#include <set>
#include <stdexcept>
#include <typeinfo>

namespace form::experimental {

  form_writer_interface::form_writer_interface(config::item_config const& config_item,
                                               config::tech_setting_config const& tech_config) :
    pers_writer_(form::detail::experimental::create_persistence_writer())
  {
    parse_config(config_item);
    pers_writer_->configure_tech_settings(tech_config);
  }

  form_writer_interface::form_writer_interface(
    config::item_config const& config_item,
    config::tech_setting_config const& tech_config,
    std::unique_ptr<form::detail::experimental::i_persistence_writer> pers_writer) :
    pers_writer_(std::move(pers_writer))
  {
    if (!pers_writer_) {
      throw std::runtime_error(
        "form_writer_interface: injected persistence writer must not be null");
    }
    parse_config(config_item);
    pers_writer_->configure_tech_settings(tech_config);
  }

  void form_writer_interface::parse_config(config::item_config const& config_item)
  {
    // Parse the product configuration exactly once: collect every configured destination for each
    // product (a product may be written to more than one place).
    for (auto const& item : config_item.get_items()) {
      config_by_product_[item.product_name].push_back(item);
    }
  }

  void form_writer_interface::write(std::string const& creator,
                                    std::string const& segment_id,
                                    product_with_name const& product)
  {
    write(creator, segment_id, std::vector<product_with_name>{product});
  }

  void form_writer_interface::write(std::string const& creator,
                                    std::string const& segment_id,
                                    std::vector<product_with_name> const& products)
  {
    using form::detail::experimental::build_full_label;
    using form::detail::experimental::placement;

    write_plan& plan = plans_[creator];

    // ---- 1. PLAN ----
    // Resolve each product to all of its placements the first time this creator writes it, and
    // create those containers.
    // Resolution is per product, not per creator: a product first seen on a later record is
    // resolved then -- as long as its destination has not been written to yet.
    // The storage backend seals a place's container structure on its first write, so a product
    // that first appears at an already-written place cannot be added there; it is rejected below.
    // FORM names only product containers; persistence adds the navigation ("index") container for
    // each place itself, so FORM stays opaque to whether a place is indexed.
    std::vector<std::pair<placement, std::type_info const*>> new_containers;
    for (auto const& pb : products) {
      auto const [places_it, is_new_product] = plan.product_places.try_emplace(pb.label);
      if (!is_new_product) {
        continue; // already resolved on an earlier write
      }

      auto const cfg_it = config_by_product_.find(pb.label);
      if (cfg_it == config_by_product_.end()) {
        // Phlex forwards every product in a store to the output module, including ones this writer
        // was never configured to persist. Leave the empty plan entry so we skip -- and log -- it
        // once rather than on every write.
        std::cerr << "No configuration found for product: " << pb.label << '\n';
        continue;
      }

      // The backend seals a place's container structure on its first write. If this product first
      // appears at a place already written to, its container can no longer be added there -- fail
      // clearly here rather than crash deep in the backend.
      for (auto const& item : cfg_it->second) {
        if (plan.sealed_places.contains(std::make_pair(item.file_name, item.technology))) {
          throw std::runtime_error(
            "form_writer_interface: product '" + pb.label + "' from creator '" + creator +
            "' first appeared after data was already written to '" + item.file_name +
            "'; the storage backend seals a container's structure on first write, so products "
            "cannot be added to it later");
        }
      }

      // A product may be configured for several destinations; build a placement for each.
      auto& places = places_it->second;
      for (auto const& item : cfg_it->second) {
        placement product_place{
          item.file_name, build_full_label(creator, pb.label), item.technology};
        new_containers.emplace_back(product_place, pb.type);
        plan.commit_places.try_emplace(std::make_pair(item.file_name, item.technology),
                                       product_place);
        places.push_back(std::move(product_place));
      }
    }

    if (!new_containers.empty()) {
      pers_writer_->create_containers(new_containers);
    }

    // ---- 2. WRITE ----
    // Fill each product into every one of its destinations, recording which places received data
    // this record. Phlex may send a different set of products from one record to the next, so FORM
    // does not assume every known place is written every record: the commit below runs only for the
    // places filled here. (Unconfigured products hold an empty placement list, so they add no
    // writes.)
    std::set<std::pair<std::string, form::technology::id>> written_places;
    for (auto const& pb : products) {
      auto const it = plan.product_places.find(pb.label);
      if (it == plan.product_places.end()) {
        continue;
      }
      for (auto const& place : it->second) {
        pers_writer_->register_write(place, pb.data, *pb.type);
        written_places.emplace(place.file_name(), place.technology());
      }
    }

    // Each place written now holds data, so its container structure is sealed: no product may be
    // added to it on a later record (enforced by the guard in PLAN above).
    plan.sealed_places.insert(written_places.begin(), written_places.end());

    // ---- 3. COMMIT ----
    // Persistence expects one commit per place per record; the commit finalizes this record's write
    // for that place.
    for (auto const& [place_key, commit_rep] : plan.commit_places) {
      if (!written_places.contains(place_key)) {
        continue; // nothing written to this (file, technology)
      }
      pers_writer_->commit_place(commit_rep, segment_id);
    }
  }
}
