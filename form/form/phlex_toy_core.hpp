// Copyright (C) 2025 ...

#ifndef __PHLEX_TOY_CORE_H__
#define __PHLEX_TOY_CORE_H__

#include <string>
#include <typeindex>
#include <vector>

namespace phlex {
  struct product_base {
    std::string label; // Containing algorithm and data product name?
    std::string id;    // this has to be more complex, including Family
    const void* data;  // this becomes a real type
    std::type_index type;
  };
};

#endif
