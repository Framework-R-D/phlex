// Copyright (C) 2025 ...

#ifndef __FORM_H__
#define __FORM_H__

#include "phlex_toy_core.hpp" // FORM Interface may include core phlex modules

#include "persistence/ipersistence.hpp"

#include <memory>

namespace form::experimental {
  class form_interface {
  public:
    form_interface();
    ~form_interface() = default;

    void write(const phlex::product_base& pb, const std::string& type);
    void read(phlex::product_base& pb, std::string& type);

  private:
    std::unique_ptr<IPersistence> m_pers;
  };
}

#endif
