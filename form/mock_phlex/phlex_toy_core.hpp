// Copyright (C) 2025 ...

#ifndef PHLEX_TOY_CORE_HPP
#define PHLEX_TOY_CORE_HPP

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace phlex::testing {
  struct product_base {
    std::string label; // Containing data product name only?
    std::string id;
    const void* data;
    std::type_index type;
  };
  struct product_type_names {
    std::unordered_map<std::type_index, std::string>
      names; // Phlex has to provide product type name
  };
  std::shared_ptr<product_type_names> createTypeMap();
};

#endif
