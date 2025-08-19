// Copyright (C) 2025 ...

#ifndef __FORM_HPP__
#define __FORM_HPP__

#include "form/parse_config.hpp"
#include "mock_phlex/phlex_toy_config.hpp"
#include "mock_phlex/phlex_toy_core.hpp"
#include "persistence/ipersistence.hpp"

#include <memory>
#include <string>

namespace form::experimental {
  class form_interface {
  public:
    form_interface(std::shared_ptr<mock_phlex::product_type_names> tm,
                   mock_phlex::config::parse_config const& phlex_config); // Accept phlex config
    ~form_interface() = default;

    void write(std::string const& creator, mock_phlex::product_base const& pb);
    void write(std::string const& creator,
               std::vector<mock_phlex::product_base> const& batch); // batch version
    void read(std::string const& creator, mock_phlex::product_base& pb);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::shared_ptr<mock_phlex::product_type_names> m_type_map;
    form::experimental::config::parse_config m_config;
  };
}

#endif
