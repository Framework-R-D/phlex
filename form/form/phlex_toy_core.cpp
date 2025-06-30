// Copyright (C) 2025 ...

#include "phlex_toy_core.hpp"

namespace phlex {
  std::shared_ptr<product_type_names> createTypeMap()
  {
    return std::shared_ptr<product_type_names>(new product_type_names());
  }
}
