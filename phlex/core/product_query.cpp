#include "phlex/core/product_query.hpp"

#include "fmt/format.h"

#include <stdexcept>

namespace phlex {
  product_query::product_query() = default;

  product_query::product_query(experimental::product_specification spec, std::string layer) :
    spec_{std::move(spec)}, layer_{std::move(layer)}
  {
  }

  product_query experimental::product_tag::operator()(std::string data_layer) &&
  {
    if (data_layer.empty()) {
      throw std::runtime_error("Cannot specify the empty string as a data layer.");
    }
    return {std::move(spec), std::move(data_layer)};
  }

  std::string product_query::to_string() const
  {
    if (layer_.empty()) {
      return spec_.full();
    }
    return fmt::format("{} ϵ {}", spec_.full(), layer_);
  }

  experimental::product_tag operator""_in(char const* product_name, std::size_t length)
  {
    if (length == 0ull) {
      throw std::runtime_error("Cannot specify product with empty name.");
    }
    return {experimental::product_specification::create(product_name)};
  }
}
