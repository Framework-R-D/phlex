#include "phlex/model/products.hpp"

#include "boost/core/demangle.hpp"

#include <string>

namespace phlex::experimental {
  auto products::contains(std::string const& product_name) const -> bool
  {
    return products_.contains(product_name);
  }

  auto products::begin() const noexcept -> products::const_iterator { return products_.begin(); }
  auto products::end() const noexcept -> products::const_iterator { return products_.end(); }

  auto products::error_message(std::string const& product_name,
                                      char const* requested_type,
                                      char const* available_type) -> std::string
  {
    return "Cannot get product '" + product_name + "' with type '" +
           boost::core::demangle(requested_type) + "' -- must specify type '" +
           boost::core::demangle(available_type) + "'.";
  }
}
