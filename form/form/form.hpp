// Copyright (C) 2025 ...

#ifndef __FORM_HPP__
#define __FORM_HPP__

#include "form/config.hpp"
#include "persistence/ipersistence.hpp"

#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace form::experimental {

  // Type map - moved from mock_phlex (FORM needs this)
  struct product_type_names {
    std::unordered_map<std::type_index, std::string> names;
  };

  // Product structure - moved from mock_phlex, but without id field
  struct product_with_name {
    std::string label;        // product name
    void const* data;         // pointer to actual data
    std::type_index type;     // type information
    // Note: id field removed - now passed separately to write()
  };

  class form_interface {
  public:
    // Constructor accepts FORM config structures (similar to old mock_phlex::config::parse_config)
    form_interface(std::shared_ptr<product_type_names> tm,
                   config::output_item_config const& output_config,
                   config::tech_setting_config const& tech_config);
    ~form_interface() = default;

    // Write single product - NEW: segment_id passed separately
    void write(std::string const& creator,
               std::string const& segment_id,
               product_with_name const& product);
    
    // Write multiple products - NEW: segment_id passed separately
    void write(std::string const& creator,
               std::string const& segment_id,
               std::vector<product_with_name> const& products);
    
    // Read product - NEW: segment_id passed separately
    void read(std::string const& creator,
              std::string const& segment_id,
              product_with_name& product);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::shared_ptr<product_type_names> m_type_map;
    // Fast lookup maps built once in constructor
    std::map<std::string, form::experimental::config::PersistenceItem> m_product_to_config;
  };
}

#endif
