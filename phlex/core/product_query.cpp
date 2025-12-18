#include "phlex/core/product_query.hpp"

#include "fmt/format.h"

#include <ostream>
#include <stdexcept>
#include <tuple>

namespace phlex::experimental {
  product_query product_tag::operator()(std::string data_layer) &&
  {
    if (data_layer.empty()) {
      throw std::runtime_error("Cannot specify the empty string as a data layer.");
    }
    return {std::move(spec), std::move(data_layer)};
  }

  std::ostream& operator<<(std::ostream& os, product_query const& query)
  {
    os << query.to_string();
    return os;
  }
}

namespace phlex {
  std::string product_query::to_string() const
  {
    if (layer.empty()) {
      return spec.full();
    }
    return fmt::format("{} ϵ {}", spec.full(), layer);
  }

  bool operator==(product_query const& a, product_query const& b)
  {
    return std::tie(a.spec, a.layer) == std::tie(b.spec, b.layer);
  }

  bool operator!=(product_query const& a, product_query const& b) { return !(a == b); }

  bool operator<(product_query const& a, product_query const& b)
  {
    return std::tie(a.spec, a.layer) < std::tie(b.spec, b.layer);
  }

  experimental::product_tag operator""_in(char const* product_name, std::size_t length)
  {
    if (length == 0ull) {
      throw std::runtime_error("Cannot specify product with empty name.");
    }
    return {experimental::product_specification::create(product_name)};
  }
}
