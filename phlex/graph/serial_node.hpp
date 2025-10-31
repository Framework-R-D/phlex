#ifndef PHLEX_GRAPH_SERIAL_NODE_HPP
#define PHLEX_GRAPH_SERIAL_NODE_HPP

#include "phlex/graph/serializer_node.hpp"
#include "phlex/utilities/sized_tuple.hpp"

#include "oneapi/tbb/flow_graph.h"

namespace phlex::experimental {

  template <typename Input>
  using base = tbb::flow::composite_node<std::tuple<Input>, std::tuple<Input>>;

  namespace detail {
    template <typename Input, std::size_t N>
    using join_tuple = concatenated_tuples<std::tuple<Input>, sized_tuple<token_t, N>>;
  }

  template <typename Input, std::size_t N>
  class serial_node : public base<Input> {
    template <typename Serializers, std::size_t... I>
    void make_edges(std::index_sequence<I...>, Serializers const& serializers)
    {
      (make_edge(std::get<I>(serializers), input_port<I + 1>(join_)), ...);
    }

    template <typename FT, typename Serializers, std::size_t... I>
    explicit serial_node(tbb::flow::graph& g,
                         std::size_t concurrency,
                         FT f,
                         Serializers serializers,
                         std::index_sequence<I...> iseq) :
      base<Input>{g},
      buffered_msgs_{g},
      join_{g},
      serialized_function_{g,
                           concurrency,
                           [serialized_resources = std::move(serializers), function = std::move(f)](
                             detail::join_tuple<Input, N> const& tup) mutable {
                             (void)serialized_resources; // To silence unused warning when N == 0
                             auto input = std::get<0>(tup);
                             function(input);
                             (std::get<I>(serialized_resources).try_put(1), ...);
                             return input;
                           }}
    {
      // Need way to route null messages around the join.
      make_edge(buffered_msgs_, input_port<0>(join_));
      make_edges(iseq, serializers);
      make_edge(join_, serialized_function_);
      base<Input>::set_external_ports(
        typename base<Input>::input_ports_type{buffered_msgs_},
        typename base<Input>::output_ports_type{serialized_function_});
    }

  public:
    template <typename FT>
    explicit serial_node(tbb::flow::graph& g, std::size_t concurrency, FT f) :
      serial_node{g, concurrency, std::move(f), std::tuple{}, std::make_index_sequence<0>{}}
    {
    }

    template <typename FT, typename... Serializers>
    explicit serial_node(tbb::flow::graph& g,
                         std::tuple<Serializers&...> const& serializers,
                         FT f) :
      serial_node{g,
                  (sizeof...(Serializers) > 0 ? tbb::flow::serial : tbb::flow::unlimited),
                  std::move(f),
                  serializers,
                  std::make_index_sequence<sizeof...(Serializers)>{}}
    {
    }

  private:
    tbb::flow::buffer_node<Input> buffered_msgs_;
    tbb::flow::join_node<detail::join_tuple<Input, N>, tbb::flow::reserving> join_;
    tbb::flow::function_node<detail::join_tuple<Input, N>, Input> serialized_function_;
  };
}

#endif // PHLEX_GRAPH_SERIAL_NODE_HPP
