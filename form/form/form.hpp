// Copyright (C) 2025 ...

#ifndef __FORM_H__
#define __FORM_H__

#include "parse_config.hpp"
#include "phlex_toy_core.hpp"
#include "phlex_toy_config.hpp"
#include "persistence/ipersistence.hpp"

#include <memory>
#include <string>

namespace form::experimental {
  class form_interface {
  public:
    form_interface(std::shared_ptr<phlex::product_type_names> tm,
                   const phlex::config::parse_config& config);
    ~form_interface() = default;

    void write(const std::string& creator, const phlex::product_base& pb);
    void write(const std::string& creator,
               const std::vector<phlex::product_base>& batch); // batch version
    void read(const std::string& creator, phlex::product_base& pb);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::shared_ptr<phlex::product_type_names> m_type_map;
    form::experimental::config::parse_config m_config;
    // Fast lookup maps built once in constructor
    std::map<std::string, form::experimental::config::PersistenceItem> m_product_to_config;
  };
}

#endif
