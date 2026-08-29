// Copyright (C) 2025 ...

#include "form_writer.hpp"

#include <iostream>
#include <typeinfo>
#include <utility>
#include <vector>

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

    auto const [plan_it, is_new_creator] = plans_.try_emplace(creator);
    write_plan& plan = plan_it->second;

    // ---- 1. PLAN ----
    // Resolve every configured product to all of its placements and create all containers in one
    // shot -- the products and the index share a container set that must exist before any fill.
    if (is_new_creator) {
      std::vector<std::pair<placement, std::type_info const*>> new_containers;
      for (auto const& pb : products) {
        auto const cfg_it = config_by_product_.find(pb.label);
        if (cfg_it == config_by_product_.end()) {
          // Phlex forwards every product in a store to the output module, including ones this writer
          // was never configured to persist. Skip those rather than treating them as an error.
          std::cerr << "No configuration found for product: " << pb.label << '\n';
          continue;
        }

        // A product may be configured for several destinations; build a placement for each.
        auto& places = plan.product_places[pb.label];
        for (auto const& item : cfg_it->second) {
          placement product_place{
            item.file_name, build_full_label(creator, pb.label), item.technology};
          new_containers.emplace_back(product_place, pb.type);
          places.push_back(std::move(product_place));

          // The index (navigation) container goes alongside the product, in the same file and
          // technology. Products sharing a place share one index; each place is committed on its
          // own, so each needs its own index.
          auto const [idx_it, inserted] = plan.index_places.try_emplace(
            std::make_pair(item.file_name, item.technology),
            placement{item.file_name, build_full_label(creator, "index"), item.technology});
          if (inserted) {
            new_containers.emplace_back(idx_it->second, &typeid(std::string));
          }
        }
      }

      if (!new_containers.empty()) {
        pers_writer_->create_containers(new_containers);
      }
    }

    // ---- 2. WRITE ----
    // Fill each product into every one of its destinations; unconfigured products are not in the
    // plan.
    for (auto const& pb : products) {
      auto const it = plan.product_places.find(pb.label);
      if (it == plan.product_places.end()) {
        continue; // unconfigured product, not part of this creator's plan
      }
      for (auto const& place : it->second) {
        pers_writer_->register_write(place, pb.data, *pb.type);
      }
    }

    // ---- 3. COMMIT ----
    // Record this segment in every destination place's index; committing a place is what writes its
    // row (RNTuple/TTree only finalize an entry on commit), so each place must be committed.
    for (auto const& [place_key, index_place] : plan.index_places) {
      pers_writer_->commit_output(index_place, segment_id);
    }
  }
}
