// Copyright (C) 2025 ...

#ifndef __FORM_H__
#define __FORM_H__

#include "form/config.hpp"
#include "mock_phlex/phlex_toy_config.hpp"
#include "mock_phlex/phlex_toy_core.hpp" // FORM Interface may include core phlex modules
#include "persistence/ipersistence.hpp"

#include <memory>
#include <string>

namespace form::experimental {
  class form_interface {
  public:
    form_interface(std::shared_ptr<mock_phlex::product_type_names> tm,
                   const mock_phlex::config::parse_config& config);
    ~form_interface() = default;

    void write(const std::string& creator, const mock_phlex::product_base& pb);
    void write(const std::string& creator,
               const std::vector<mock_phlex::product_base>& batch); // batch version
    void read(const std::string& creator, mock_phlex::product_base& pb);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::shared_ptr<mock_phlex::product_type_names> m_type_map;
    form::experimental::config::output_item_config m_output_items;
    // Fast lookup maps built once in constructor
    std::map<std::string, form::experimental::config::PersistenceItem> m_product_to_config;
  };
}

#endif
