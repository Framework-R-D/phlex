#include "phlex/core/product_query.hpp"

#include "fmt/format.h"

#include <ostream>
#include <stdexcept>
#include <tuple>

namespace phlex::experimental {
  product_query product_query::operator()(std::string layer) &&
  {
    return {std::move(name), std::move(layer)};
  }

  std::string product_query::to_string() const
  {
    if (layer.empty()) {
      return name.full();
    }
    return fmt::format("{} ϵ {}", name.full(), layer);
  }

  product_query operator""_in(char const* name, std::size_t length)
  {
    if (length == 0ull) {
      throw std::runtime_error("Cannot specify product with empty name.");
    }
    return product_query::create(name);
  }

  bool operator==(product_query const& a, product_query const& b)
  {
    return std::tie(a.name, a.layer) == std::tie(b.name, b.layer);
  }

  bool operator!=(product_query const& a, product_query const& b) { return !(a == b); }

  bool operator<(product_query const& a, product_query const& b)
  {
    return std::tie(a.name, a.layer) < std::tie(b.name, b.layer);
  }

  std::ostream& operator<<(std::ostream& os, product_query const& label)
  {
    os << label.to_string();
    return os;
  }

  product_query product_query::create(char const* c) { return create(std::string{c}); }

  product_query product_query::create(std::string const& s)
  {
    return {product_specification::create(s)};
  }

  product_query product_query::create(product_query l) { return l; }
}
