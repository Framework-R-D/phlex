// Copyright (C) 2025 ...

#ifndef __FORM_H__
#define __FORM_H__

#include "phlex_toy_core.hpp" // FORM Interface may include core phlex modules

#include "persistence/ipersistence.hpp"

#include <memory>
#include <string>

namespace form::experimental {
  class form_interface {
  public:
    form_interface(std::shared_ptr<phlex::product_type_names> tm);
    ~form_interface() = default;

    void write(const std::string& creator, const phlex::product_base& pb);
    void write(const std::string& creator,
               const std::vector<phlex::product_base>& batch); // batch version
    void read(phlex::product_base& pb);

  private:
    std::unique_ptr<form::detail::experimental::IPersistence> m_pers;
    std::shared_ptr<phlex::product_type_names> m_type_map;
  };
}

#endif
