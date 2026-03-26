#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace phlex::experimental {

  std::size_t message_matcher::operator()(message const& msg) const noexcept { return msg.id; }

  message const& more_derived(message const& a, message const& b)
  {
    assert(a.store);
    assert(b.store);
    if (a.store->index()->depth() > b.store->index()->depth()) {
      return a;
    }
    return b;
  }

  std::size_t port_index_for(product_queries const& input_products,
                             product_query const& input_product)
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
