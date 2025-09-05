// Copyright (C) 2025 ...

#ifndef __PHLEX_TOY_CORE_HPP__
#define __PHLEX_TOY_CORE_HPP__

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace mock_phlex {
  struct product_base {
    std::string label; // Containing data product name only?
    std::string id;
    void const* data;
    std::type_index type;
  };
  struct product_type_names {
    std::unordered_map<std::type_index, std::string>
      names; // Phlex has to provide product type name
  };
  std::shared_ptr<product_type_names> createTypeMap();
};

#endif
