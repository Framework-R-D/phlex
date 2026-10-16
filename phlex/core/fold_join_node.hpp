#ifndef PHLEX_CORE_FOLD_JOIN_NODE_HPP
#define PHLEX_CORE_FOLD_JOIN_NODE_HPP

#include "phlex/core/detail/accumulator_node.hpp"
#include "phlex/core/detail/repeater_node.hpp"
#include "phlex/core/join_registration.hpp"
#include "phlex/core/message.hpp"

#include <gsl/assert>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace phlex::detail {

  // A fold_join_node is a TBB composite flow-graph node that synchronises a
  // partition-level fold accumulator with N data-input streams.  For each data
  // unit it waits until the accumulator_node has emitted the current partial
  // result *and* all N repeater_nodes have forwarded their data messages, then
  // emits the complete tuple (accumulator, data[0..N-1]) to the fold executor.
  //
  // The accumulator_node manages fold results at the partition layer: it receives
  // a partition-level index and, for every expected data cell, emits an
  // accumulator_message carrying the shared partial result.
  //
  // A repeater_node is always present for each data input because the
  // partition-layer accumulator necessarily lives at a higher hierarchy level than
  // the data products being folded.  Repeaters are therefore never collapsed away
  // as they are in multilayer_join_node.
  //
  // Schematic:
  //
  //   partition ──► accumulator_node ──► port<0>(join) ──┐
  //                                                      │
  //      data[0] ──► repeater[0] ──────► port<1>(join) ──┤
  //             ⋮                                        ├──► join_node ──► (accum, data[0..N-1])
  //   data[N-1] ──► repeater[N-1] ────► port<N>(join) ───┘

  template <std::size_t NProducts>
  using partition_index_with_messages =
    decltype(std::tuple_cat(std::tuple<index_message>{}, message_tuple<NProducts>{}));

  template <typename FoldResult, std::size_t NProducts>
  using accumulator_with_messages = decltype(std::tuple_cat(
    std::tuple<internal::accumulator_message<FoldResult>>{}, message_tuple<NProducts>{}));

  template <typename FoldResult, std::size_t NProducts>
  using fold_join_node_base_t =
    tbb::flow::composite_node<partition_index_with_messages<NProducts>,
                              std::tuple<accumulator_with_messages<FoldResult, NProducts>>>;

  template <typename FoldResult, std::size_t NProducts>
  class fold_join_node : public fold_join_node_base_t<FoldResult, NProducts> {
    using args_t = partition_index_with_messages<NProducts>;
    using base_t = fold_join_node_base_t<FoldResult, NProducts>;
    using join_args_t = accumulator_with_messages<FoldResult, NProducts>;
    using result_initializer_t = std::function<std::unique_ptr<FoldResult>(data_cell_index const&)>;
    using input_t = base_t::input_ports_type;
    using output_t = base_t::output_ports_type;

    static_assert(NProducts >= 1, "fold_join_node requires at least one data product.");

    template <std::size_t... Is>
    static auto make_join(tbb::flow::graph& g, std::index_sequence<Is...>)
    {
      // tag_matching causes TBB to group messages by the value returned by
      // message_matcher, which extracts the message ID.  Messages with the same ID across
      // all ports are forwarded together as one tuple, ensuring that each output contains
      // exactly the messages that belong to the same data unit.
      return tbb::flow::join_node<join_args_t, tbb::flow::tag_matching>{
        g,
        [](internal::accumulator_message<FoldResult> const& msg) { return msg.id; },
        type_t<message_matcher, Is>{}...};
    }

  public:
    fold_join_node(tbb::flow::graph& g,
                   phlex::experimental::algorithm_name const& node_name,
                   phlex::experimental::identifier const& stage,
                   phlex::experimental::identifier const& partition_layer_name,
                   std::vector<phlex::experimental::identifier> layer_names,
                   phlex::experimental::product_specifications output,
                   result_initializer_t result_initializer) :
      base_t{g},
      result_repeater_{g,
                       node_name,
                       stage,
                       partition_layer_name,
                       std::move(output),
                       std::move(result_initializer)},
      join_{make_join(g, std::make_index_sequence<NProducts>{})},
      name_{node_name},
      partition_layer_{partition_layer_name},
      layers_{std::move(layer_names)}
    {
      Expects(NProducts == layers_.size());

      // We do not worry about collapsing layers for the fold-join node as the accumulator
      // repeater will always belong to a higher layer than (at least one of) the data products
      // that serve as input to the fold operation.
      repeaters_.reserve(NProducts);
      for (auto const& layer : layers_) {
        repeaters_.push_back(std::make_unique<internal::repeater_node>(g, name_, layer));
      }

      make_edge(tbb::flow::output_port<1>(result_repeater_), input_port<0>(join_));
      auto set_ports = [this]<std::size_t... Is>(std::index_sequence<Is...>) {
        this->set_external_ports(
          input_t{result_repeater_.partition_port(), repeaters_[Is]->data_port()...},
          output_t{join_});
        // Connect repeaters to join
        (make_edge(*repeaters_[Is], input_port<Is + 1>(join_)), ...);
      };

      set_ports(std::make_index_sequence<NProducts>{});
    }

    tbb::flow::sender<message>& output_port()
    {
      return tbb::flow::output_port<0>(result_repeater_);
    }
    tbb::flow::receiver<std::size_t>& notify_result_repeater_port()
    {
      return input_port<3>(result_repeater_);
    }

    // The partition constrains the product-driven invocation domain. Its initialization
    // connection remains separate from the accumulator's per-invocation index receiver.
    // Ancestor data repeaters and the accumulator complete against the same invocations.
    join_registration index_ports()
    {
      join_registration result{.input_layers = layers_, .participants = {}};
      result.participants.reserve(1 + repeaters_.size()); // +1 for the result repeater
      result.participants.push_back({.layer = partition_layer_,
                                     .index_port = &result_repeater_.index_port(),
                                     .completion_port = &result_repeater_.flush_port(),
                                     .partition_port = &result_repeater_.partition_port()});
      for (std::size_t i = 0; i != repeaters_.size(); ++i) {
        result.participants.push_back({.layer = layers_[i],
                                       .index_port = &repeaters_[i]->index_port(),
                                       .completion_port = &repeaters_[i]->flush_port(),
                                       .input_ordinal = i});
      }
      return result;
    }

  private:
    internal::accumulator_node<FoldResult> result_repeater_;
    std::vector<std::unique_ptr<internal::repeater_node>> repeaters_;
    tbb::flow::join_node<join_args_t, tbb::flow::tag_matching> join_;
    // Immutable after construction; tbb::flow::join_node is already non-movable.
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    phlex::experimental::algorithm_name const name_;
    phlex::experimental::identifier const partition_layer_;
    std::vector<phlex::experimental::identifier> const layers_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  // Translates a runtime port index into a reference to the corresponding compile-time
  // input port by recursively incrementing the compile-time parameter I until it matches
  // the runtime index.
  template <std::size_t I, typename FoldResult, std::size_t N>
  tbb::flow::receiver<message>& receiver_for(fold_join_node<FoldResult, N>& join,
                                             std::size_t const index)
  {
    if constexpr (I < N + 1) { // +1 to account for the result repeater port at index 0
      if (I != index + 1ull) { // +1 to account for the result repeater port at index 0
        return receiver_for<I + 1ull>(join, index);
      }
      return input_port<I>(join);
    }
    throw std::runtime_error("Should never get here");
  }

  namespace detail {
    // Looks up the port index for the given input product query, then returns a reference to
    // the corresponding input port of the join node.  Only valid for N > 1.
    template <typename FoldResult, std::size_t N>
    tbb::flow::receiver<message>& receiver_for(fold_join_node<FoldResult, N>& join,
                                               product_selectors const& input_products,
                                               product_selector const& input_product)
    {
      auto const index = port_index_for(input_products, input_product);
      return receiver_for<1ull>(join, index); // Start at 1 to skip the result repeater port
    }
  }

  // Returns the receiver for the input port that corresponds to the given input product query.
  // For N == 1 there is only one port so the node itself is returned; for N > 1 the port is
  // looked up by query within the join.
  template <typename FoldResult, std::size_t N>
  tbb::flow::receiver<message>& receiver_for(fold_join_node<FoldResult, N>& join,
                                             product_selectors const& input_products,
                                             product_selector const& input_product)
  {
    return detail::receiver_for(join, input_products, input_product);
  }
}

#endif // PHLEX_CORE_FOLD_JOIN_NODE_HPP
