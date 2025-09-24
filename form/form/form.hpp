// Copyright (C) 2025 ...

#ifndef __FORM_HPP__
#define __FORM_HPP__

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
                   mock_phlex::config::parse_config const& config);
    ~form_interface() = default;

    void write(std::string const& creator, mock_phlex::product_base const& pb);
    void write(std::string const& creator,
               std::vector<mock_phlex::product_base> const& batch); // batch version
    void read(std::string const& creator, mock_phlex::product_base& pb);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::shared_ptr<mock_phlex::product_type_names> m_type_map;
    // Fast lookup maps built once in constructor
    std::map<std::string, form::experimental::config::PersistenceItem> m_product_to_config;
  };
}

#endif
