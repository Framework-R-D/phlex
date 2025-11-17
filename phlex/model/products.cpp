#include "phlex/model/products.hpp"

#include "boost/core/demangle.hpp"

#include <string>

namespace phlex::experimental {
  bool products::contains(std::string const& product_name) const
  {
    return products_.contains(product_name);
  }

  products::const_iterator products::begin() const noexcept { return products_.begin(); }
  products::const_iterator products::end() const noexcept { return products_.end(); }

  void products::throw_mismatched_type(std::string const& product_name,
                                       char const* requested_type,
                                       char const* available_type)
  {
    throw std::runtime_error("Cannot get product '" + product_name + "' with type '" +
                             boost::core::demangle(requested_type) + "' -- must specify type '" +
                             boost::core::demangle(available_type) + "'.");
  }
}
