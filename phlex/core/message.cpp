#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_index.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace phlex::experimental {

  std::size_t MessageHasher::operator()(message const& msg) const noexcept { return msg.id; }

  message const& more_derived(message const& a, message const& b)
  {
    assert(a.store);
    assert(b.store);
    if (a.store->id()->depth() > b.store->id()->depth()) {
      return a;
    }
    return b;
  }

  std::size_t port_index_for(product_queries const& product_labels,
                             product_query const& product_label)
  {
    auto const [b, e] = std::tuple{cbegin(product_labels), cend(product_labels)};
    auto it = std::find(b, e, product_label);
    if (it == e) {
      throw std::runtime_error("Algorithm does not accept product '" + product_label.spec().name() +
                               "'.");
    }
    return std::distance(b, it);
  }

  detail::no_join::no_join(tbb::flow::graph& g, MessageHasher) :
    no_join_base_t{g, tbb::flow::unlimited, [](message const& msg) { return std::tuple{msg}; }}
  {
  }
}
