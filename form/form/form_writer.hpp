// Copyright (C) 2025 ...

#ifndef FORM_FORM_FORM_WRITER_HPP
#define FORM_FORM_FORM_WRITER_HPP

#include "core/container_naming.hpp"
#include "core/placement.hpp"
#include "form/config.hpp"
#include "form/product_with_name.hpp"
#include "persistence/ipersistence_writer.hpp"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace form::experimental {

  // FORM owns the product configuration: it parses the config once, resolves each product to all
  // of its destination placements the first time a creator writes, creates those containers once,
  // and then just routes writes.
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
      // product label -> all of its configured destination placements (a product may fan out to
      // several files/backends)
      std::unordered_map<std::string, std::vector<form::detail::experimental::placement>>
        product_places;
      // Committing is once per destination place (file + technology).
      std::map<std::pair<std::string, form::technology::id>, form::detail::experimental::placement>
        commit_places;
    };

    void parse_config(config::item_config const& config_item);

    std::unique_ptr<form::detail::experimental::i_persistence_writer> pers_writer_;
    // product label -> all of its configured destinations (parsed once, at construction)
    std::unordered_map<std::string, std::vector<config::persistence_item>> config_by_product_;
    // creator -> its resolved write plan (built lazily on first write)
    std::unordered_map<std::string, write_plan> plans_;
  };
}

#endif // FORM_FORM_FORM_WRITER_HPP
