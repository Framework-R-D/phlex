#include "phlex/core/message.hpp"
#include "phlex/model/level_id.hpp"

#include <algorithm>
#include <iterator>
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

  std::size_t port_index_for(specified_labels product_labels, specified_label const& product_label)
  {
    auto const [b, e] = std::tuple{cbegin(product_labels), cend(product_labels)};
    auto it = std::find(b, e, product_label);
    if (it == e) {
      throw std::runtime_error("Algorithm does not accept product '" + product_label.name.name() +
                               "'.");
    }
    return std::distance(b, it);
  }
}
