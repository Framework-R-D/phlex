#include "phlex/core/product_query.hpp"

#include "fmt/format.h"

namespace phlex {
   void product_query::set_type(experimental::type_id&& type) {
     type_id_ = std::move(type);
   }
  
  std::string product_query::to_string() const
  {
    if (suffix_) {
      return fmt::format("{}/{} ϵ {}", creator_, *suffix_, layer_);
    }
    return fmt::format("{} ϵ {}", creator_, layer_);
  }
}
