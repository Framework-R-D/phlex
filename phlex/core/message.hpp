#ifndef PHLEX_CORE_MESSAGE_HPP
#define PHLEX_CORE_MESSAGE_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/sized_tuple.hpp"

#include "oneapi/tbb/flow_graph.h" // <-- belongs somewhere else

#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>

namespace phlex::experimental {

  struct index_message {
    data_cell_index_ptr index;
    std::size_t msg_id;
    bool cache{true};
  };

  // FIXME: Do we need both indexed_end_token and flush_message?
  struct indexed_end_token {
    data_cell_index_ptr index;
    int count;
  };

  struct flush_message {
    data_cell_index_ptr index;
    flush_counts_ptr counts;
    std::size_t original_id; // FIXME: Used only by folds
  };

  struct message {
    // FIXME: Maybe consider adding an 'index' data member?
    product_store_const_ptr store;
    std::size_t id;
  };

  struct message_matcher {
    std::size_t operator()(message const& msg) const noexcept;
  };

  template <std::size_t N>
  using messages_t = sized_tuple<message, N>;

  // Overload for use with most_derived
  message const& more_derived(message const& a, message const& b);

  std::size_t port_index_for(product_queries const& product_labels,
                             product_query const& product_label);
}

#endif // PHLEX_CORE_MESSAGE_HPP
