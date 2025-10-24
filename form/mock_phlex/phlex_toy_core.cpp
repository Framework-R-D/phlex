// Copyright (C) 2025 ...

#include "phlex_toy_core.hpp"
#include <memory>

namespace mock_phlex {
  auto createTypeMap() -> std::shared_ptr<product_type_names>
  {
    return std::make_shared<product_type_names>();
  }
}
