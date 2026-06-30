#ifndef PHLEX_CORE_MESSAGE_HPP
#define PHLEX_CORE_MESSAGE_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/fwd.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/sized_tuple.hpp"

#include "oneapi/tbb/flow_graph.h" // <-- belongs somewhere else

#include <cstddef>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace phlex::detail {

  struct index_message {
    data_cell_index_ptr index;
    std::size_t msg_id{};
    bool cache{true};
  };

  // FIXME: Do we need both indexed_end_token and flush_message?
  struct indexed_end_token {
    data_cell_index_ptr index;
    int count;
  };

  struct flush_message {
    data_cell_index_ptr index;
    phlex::detail::data_cell_counts_const_ptr counts;
    std::size_t original_id{}; // FIXME: Used only by folds
  };

  struct message {
    // FIXME: Maybe consider adding an 'index' data member?
    phlex::experimental::product_store_const_ptr store;
    std::size_t id{};
  };

  struct PHLEX_CORE_EXPORT message_matcher {
    std::size_t operator()(message const& msg) const noexcept;
  };

  template <std::size_t N>
  using message_tuple = phlex::detail::sized_tuple<message, N>;

  template <std::size_t N>
  using messages_t = std::conditional_t<N == 1ull, message, message_tuple<N>>;

  struct named_index_port {
    phlex::experimental::identifier layer;
    tbb::flow::receiver<indexed_end_token>* token_port;
    tbb::flow::receiver<index_message>* index_port;
  };
  using named_index_ports = std::vector<named_index_port>;

  // Overload for use with most_derived
  PHLEX_CORE_EXPORT message const& more_derived(message const& a, message const& b);

  // Non-template overload for single message case
  inline message const& most_derived(message const& msg)
  {
    return msg; // NOLINT(bugprone-return-const-ref-from-parameter)
  }

  // Generic most_derived for message tuples
  template <std::size_t I, typename Tuple>
  auto const& get_most_derived(Tuple const& tup, std::tuple_element_t<I - 1, Tuple> const& element)
  {
    constexpr auto num_inputs = std::tuple_size_v<Tuple>;
    if constexpr (I == num_inputs - 1) {
      return more_derived(element, std::get<I>(tup));
    } else {
      return get_most_derived<I + 1>(tup, more_derived(element, std::get<I>(tup)));
    }
  }

  template <typename T, typename U, typename... Ts>
  auto const& most_derived(std::tuple<T, U, Ts...> const& elements)
  {
    return get_most_derived<1ull>(elements, std::get<0>(elements));
  }

  PHLEX_CORE_EXPORT std::size_t port_index_for(product_selectors const& input_products,
                                               product_selector const& input_product);
}

#endif // PHLEX_CORE_MESSAGE_HPP
