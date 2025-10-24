#include "phlex/core/message.hpp"
#include "phlex/core/specified_label.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <oneapi/tbb/flow_graph.h>
#include <stdexcept>
#include <tuple>

namespace phlex::experimental {

  std::size_t MessageHasher::operator()(message const& msg) const noexcept { return msg.id; }

  message const& more_derived(message const& a, message const& b)
  {
    if (a.store->id()->depth() > b.store->id()->depth()) {
      return a;
    }
    return b;
  }

  std::size_t port_index_for(specified_labels const& product_labels,
                             specified_label const& product_label)
  {
    auto const [b, e] = std::tuple{cbegin(product_labels), cend(product_labels)};
    auto it = std::find(b, e, product_label);
    if (it == e) {
      throw std::runtime_error("Algorithm does not accept product '" + product_label.name.name() +
                               "'.");
    }
    return std::distance(b, it);
  }

  detail::no_join::no_join(tbb::flow::graph& g, MessageHasher) :
    no_join_base_t{g, tbb::flow::unlimited, [](message const& msg) { return std::tuple{msg}; }}
  {
  }
}
