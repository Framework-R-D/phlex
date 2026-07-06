#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace phlex::detail {

  std::size_t message_matcher::operator()(message const& msg) const noexcept { return msg.id; }

  message const& more_derived(message const& a, message const& b)
  {
    assert(a.store);
    assert(b.store);
    auto const& store = phlex::experimental::detail::more_derived(a.store, b.store);
    if (store == a.store) {
      return a; // NOLINT(bugprone-return-const-ref-from-parameter)
    }
    return b; // NOLINT(bugprone-return-const-ref-from-parameter)
  }

  std::size_t port_index_for(product_selectors const& input_products,
                             product_selector const& input_product)
  {
    auto const [b, e] = std::tuple{cbegin(input_products), cend(input_products)};
    auto it = std::find(b, e, input_product);
    if (it == e) {
      throw std::runtime_error(
        fmt::format("Algorithm does not accept product '{}'.", input_product));
    }
    return std::distance(b, it);
  }
}
