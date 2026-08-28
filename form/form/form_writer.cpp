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
    // Parse the product configuration exactly once: map each product to its configured destination.
    for (auto const& item : config_item.get_items()) {
      config_by_product_.emplace(item.product_name, item);
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
    // Resolve every configured product to its placement and create all containers in one shot --
    // products and the index share a container set that must exist before any fill.
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

        auto const& item = cfg_it->second;
        placement product_place{
          item.file_name, build_full_label(creator, pb.label), item.technology};
        new_containers.emplace_back(product_place, pb.type);

        // The index is the creator's navigation container (a table of segments written), not a data
        // product.
        if (!plan.index_created) {
          plan.index_place =
            placement{item.file_name, build_full_label(creator, "index"), item.technology};
          new_containers.emplace_back(plan.index_place, &typeid(std::string));
          plan.index_created = true;
        }
        plan.product_places.emplace(pb.label, std::move(product_place));
      }

      if (!new_containers.empty()) {
        pers_writer_->create_containers(new_containers);
      }
    }

    // ---- 2. WRITE ----
    // Fill each configured product into its destination; unconfigured products are not in the plan.
    for (auto const& pb : products) {
      auto const it = plan.product_places.find(pb.label);
      if (it == plan.product_places.end()) {
        continue; // unconfigured product, not part of this creator's plan
      }
      pers_writer_->register_write(it->second, pb.data, *pb.type);
    }

    // ---- 3. COMMIT ----
    // Record this segment in the creator's index container.
    if (plan.index_created) {
      pers_writer_->commit_output(plan.index_place, segment_id);
    }
  }
}
