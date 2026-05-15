#include "phlex/model/products.hpp"

#include "boost/core/demangle.hpp"

#include <string>

namespace phlex::experimental {
  products::products(std::size_t number_known_products)
  {
    products_.reserve(number_known_products);
  }

  bool products::contains(product_specification const& spec) const
  {
    return std::ranges::any_of(products_, [&](auto const& p) { return p.first == spec; });
  }

  products::const_iterator products::begin() const noexcept { return products_.begin(); }
  products::const_iterator products::end() const noexcept { return products_.end(); }
  products::size_type products::size() const noexcept { return products_.size(); }
  bool products::empty() const noexcept { return products_.empty(); }

  void products::throw_mismatched_type(product_specification const& spec,
                                       char const* requested_type,
                                       char const* available_type)
  {
    std::string const msg =
      fmt::format("Cannot get product '{}' with type '{}' -- must specify type '{}'.",
                  spec.full(),
                  boost::core::demangle(requested_type),
                  boost::core::demangle(available_type));
    throw std::runtime_error(msg);
  }
}
