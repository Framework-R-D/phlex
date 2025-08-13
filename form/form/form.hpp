// Copyright (C) 2025 ...

#ifndef __FORM_H__
#define __FORM_H__

#include "mock_phlex/phlex_toy_config.hpp"
#include "mock_phlex/phlex_toy_core.hpp"
#include "parse_config.hpp"
#include "persistence/ipersistence.hpp"

#include <memory>
#include <string>

namespace form::experimental {
  class form_interface {
  public:
    form_interface(std::shared_ptr<phlex::testing::product_type_names> tm,
                   const form::experimental::config::parse_config& config);
    ~form_interface() = default;

    void write(const std::string& creator, const phlex::testing::product_base& pb);
    void write(const std::string& creator,
               const std::vector<phlex::testing::product_base>& batch); // batch version
    void read(const std::string& creator, phlex::testing::product_base& pb);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::shared_ptr<phlex::testing::product_type_names> m_type_map;
    form::experimental::config::parse_config m_config;
  };
}

#endif
