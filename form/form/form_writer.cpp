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

  form_writer_interface::~form_writer_interface()
  {
    // Safety net only; call finalize() explicitly to handle errors. Errors during destruction are
    // reported but cannot be propagated.
    try {
      finalize();
    } catch (std::exception const& e) {
      std::cerr << "form_writer_interface: finalize() failed: " << e.what() << '\n';
    } catch (...) {
      std::cerr << "form_writer_interface: finalize() failed with an unknown exception\n";
    }
  }

  void form_writer_interface::finalize()
  {
    if (finalized_) {
      return;
    }
    // Mark finalized before closing so a failed close is not retried by the destructor.
    finalized_ = true;
    pers_writer_->finalize();
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
                                    std::string const& stage,
                                    form::detail::experimental::cell_index const& cell,
                                    product_with_name const& product)
  {
    write(creator, stage, cell, std::vector<product_with_name>{product});
  }

  void form_writer_interface::write(std::string const& creator,
                                    std::string const& stage,
                                    form::detail::experimental::cell_index const& cell,
                                    std::vector<product_with_name> const& products)
  {
    using form::detail::experimental::build_full_label;
    using form::detail::experimental::build_row_space_name;
    using form::detail::experimental::placement;
    using form::detail::experimental::product_identity;

    // Writes are not allowed after finalize(): the navigation tables have already been written and
    // cannot record products written afterwards.
    if (finalized_) {
      throw std::runtime_error("form_writer_interface: creator '" + creator + "' at stage '" +
                               stage + "' wrote data cell " + cell.id +
                               " after the writer was finalized; the navigation tables are "
                               "already written and cannot record it");
    }

    // Reject duplicate product labels before writing.
    std::set<std::string> labels;
    for (auto const& pb : products) {
      if (!labels.insert(pb.label).second) {
        throw std::runtime_error("form_writer_interface: creator '" + creator + "' at stage '" +
                                 stage + "' wrote product '" + pb.label +
                                 "' more than once for data cell " + cell.id);
      }
    }

    auto const row_space = build_row_space_name(creator, stage);
    write_plan& plan = plans_[std::make_pair(creator, stage)];

    // ---- 1. PLAN ----
    // Resolve product placements and create new containers.
    // Container structure is sealed on first write.
    // Persistence manages the row-space index container.
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
          throw std::runtime_error("form_writer_interface: product '" + pb.label +
                                   "' from creator '" + creator + "' at stage '" + stage +
                                   "' first appeared after data was written to '" + item.file_name +
                                   "'; container structure is sealed on first write");
        }
      }

      // A product may be configured for several destinations; build a placement for each.
      auto& places = places_it->second;
      for (auto const& item : cfg_it->second) {
        placement product_place{
          item.file_name, build_full_label(row_space, pb.label), item.technology};
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
      product_identity const product{.creator = creator, .stage = stage, .label = pb.label};
      for (auto const& place : it->second) {
        pers_writer_->register_write(product, place, pb.data, *pb.type);
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
      pers_writer_->commit_place(commit_rep, cell);
    }
  }
}
