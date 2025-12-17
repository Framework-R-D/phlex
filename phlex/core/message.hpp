#ifndef PHLEX_CORE_MESSAGE_HPP
#define PHLEX_CORE_MESSAGE_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/product_query.hpp"
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

  struct message {
    product_store_const_ptr store;
    end_of_message_ptr eom;
    std::size_t id;
    std::size_t original_id{-1ull}; // Used during flush
  };

  template <std::size_t N>
  using messages_t = sized_tuple<message, N>;

  struct MessageHasher {
    std::size_t operator()(message const& msg) const noexcept;
  };

  // Overload for use with most_derived
  message const& more_derived(message const& a, message const& b);

  namespace detail {
    template <std::size_t N>
    using join_messages_t = tbb::flow::join_node<messages_t<N>, tbb::flow::tag_matching>;
    using no_join_base_t =
      tbb::flow::function_node<message, messages_t<1ull>, tbb::flow::lightweight>;

    struct no_join : no_join_base_t {
      no_join(tbb::flow::graph& g, MessageHasher);
    };
  }

  template <std::size_t N>
  using join_or_none_t = std::conditional_t<N == 1ull, detail::no_join, detail::join_messages_t<N>>;

  template <std::size_t... Is>
  auto make_join_or_none(tbb::flow::graph& g, std::index_sequence<Is...>)
  {
    return join_or_none_t<sizeof...(Is)>{g, type_t<MessageHasher, Is>{}...};
  }

  template <std::size_t N>
  std::vector<tbb::flow::receiver<message>*> input_ports(join_or_none_t<N>& join)
  {
    if constexpr (N == 1ull) {
      return {&join};
    } else {
      return [&join]<std::size_t... Is>(
               std::index_sequence<Is...>) -> std::vector<tbb::flow::receiver<message>*> {
        return {&input_port<Is>(join)...};
      }(std::make_index_sequence<N>{});
    }
  }

  std::size_t port_index_for(product_queries const& product_labels,
                             product_query const& product_label);

  template <std::size_t I, std::size_t N>
  tbb::flow::receiver<message>& receiver_for(detail::join_messages_t<N>& join,
                                             std::size_t const index)
  {
    if constexpr (I < N) {
      if (I != index) {
        return receiver_for<I + 1ull, N>(join, index);
      }
      return input_port<I>(join);
    }
    throw std::runtime_error("Should never get here");
  }

  template <std::size_t N>
  tbb::flow::receiver<message>& receiver_for(join_or_none_t<N>& join,
                                             product_queries const& product_labels,
                                             product_query const& product_label)
  {
    if constexpr (N > 1ull) {
      auto const index = port_index_for(product_labels, product_label);
      return receiver_for<0ull, N>(join, index);
    } else {
      return join;
    }
  }
}

#endif // PHLEX_CORE_MESSAGE_HPP
