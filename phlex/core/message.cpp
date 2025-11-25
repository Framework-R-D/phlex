#include "phlex/core/message.hpp"
#include "phlex/core/specified_label.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <oneapi/tbb/flow_graph.h>
#include <stdexcept>
#include <tuple>

namespace phlex::experimental {

  auto MessageHasher::operator()(message const& msg) const noexcept -> std::size_t { return msg.id; }

  auto more_derived(message const& a, message const& b) -> message const&
  {
    if (a.store->id()->depth() > b.store->id()->depth()) {
      return a;
    }
    return b;
  }

  auto port_index_for(specified_labels const& product_labels,
                             specified_label const& product_label) -> std::size_t
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
    no_join_base_t{g, tbb::flow::unlimited, [](message const& msg) -> std::tuple<phlex::experimental::message> { return std::tuple{msg}; }}
  {
  }
}
