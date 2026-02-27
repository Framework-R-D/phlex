// Copyright (C) 2025 ...

#ifndef FORM_FORM_FORM_HPP
#define FORM_FORM_FORM_HPP

#include "form/config.hpp"
#include "persistence/ipersistence.hpp"

#include <map>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

namespace form::experimental {

  struct product_with_name {
    std::string label;
    void const* data;
    std::type_info const* type;
  };

  class form_interface {
  public:
    form_interface(config::output_item_config const& output_config,
                   config::tech_setting_config const& tech_config);
    ~form_interface() = default;

    void write(std::string const& creator,
               std::string const& segment_id,
               product_with_name const& product);

    void write(std::string const& creator,
               std::string const& segment_id,
               std::vector<product_with_name> const& products);

    void read(std::string const& creator,
              std::string const& segment_id,
              product_with_name& product);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::map<std::string, form::experimental::config::PersistenceItem> m_product_to_config;
  };
}

#endif // FORM_FORM_FORM_HPP
