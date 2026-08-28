// Copyright (C) 2025 ...

#ifndef FORM_FORM_FORM_WRITER_HPP
#define FORM_FORM_FORM_WRITER_HPP

#include "core/placement.hpp"
#include "form/config.hpp"
#include "form/product_with_name.hpp"
#include "persistence/ipersistence_writer.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace form::experimental {

  // FORM owns the product configuration: it parses the config once, resolves each product to its
  // destination placement the first time a creator writes, creates those containers once, and
  // then just routes writes.
  class form_writer_interface {
  public:
    form_writer_interface(config::item_config const& config_item,
                          config::tech_setting_config const& tech_config);
    // Test seam: inject a persistence writer (e.g. a spy) instead of the default backend.
    form_writer_interface(
      config::item_config const& config_item,
      config::tech_setting_config const& tech_config,
      std::unique_ptr<form::detail::experimental::i_persistence_writer> pers_writer);
    ~form_writer_interface() = default;

    void write(std::string const& creator,
               std::string const& segment_id,
               product_with_name const& product);

    void write(std::string const& creator,
               std::string const& segment_id,
               std::vector<product_with_name> const& products);

  private:
    // Placements for one creator, resolved from config on first write and reused thereafter.
    struct write_plan {
      // product label -> its configured destination placement
      std::unordered_map<std::string, form::detail::experimental::placement> product_places;
      // this creator's single navigation ("index") placement, created with its first product
      form::detail::experimental::placement index_place;
      bool index_created = false;
    };

    void parse_config(config::item_config const& config_item);

    std::unique_ptr<form::detail::experimental::i_persistence_writer> pers_writer_;
    // product label -> its configured destination (parsed once, at construction)
    std::unordered_map<std::string, config::persistence_item> config_by_product_;
    // creator -> its resolved write plan (built lazily on first write)
    std::unordered_map<std::string, write_plan> plans_;
  };
}

#endif // FORM_FORM_FORM_WRITER_HPP
