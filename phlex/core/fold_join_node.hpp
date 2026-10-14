#ifndef PHLEX_CORE_FOLD_JOIN_NODE_HPP
#define PHLEX_CORE_FOLD_JOIN_NODE_HPP

#include "phlex/core/detail/accumulator_node.hpp"
#include "phlex/core/detail/join_support.hpp"
#include "phlex/core/message.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <cassert>
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

  template <std::size_t NInputs>
  using partition_index_with_messages =
    decltype(std::tuple_cat(std::tuple<index_message>{}, message_tuple<NInputs>{}));

  template <typename FoldResult, std::size_t NInputs>
  using accumulator_with_messages = decltype(std::tuple_cat(
    std::tuple<internal::accumulator_message<FoldResult>>{}, message_tuple<NInputs>{}));

  template <typename FoldResult, std::size_t NInputs>
  using fold_join_node_base_t =
    tbb::flow::composite_node<partition_index_with_messages<NInputs>,
                              std::tuple<accumulator_with_messages<FoldResult, NInputs>>>;

  template <typename FoldResult, std::size_t NInputs>
  class fold_join_node : public fold_join_node_base_t<FoldResult, NInputs> {
    using args_t = partition_index_with_messages<NInputs>;
    using base_t = fold_join_node_base_t<FoldResult, NInputs>;
    using join_args_t = accumulator_with_messages<FoldResult, NInputs>;
    using result_initializer_t = std::function<std::unique_ptr<FoldResult>(data_cell_index const&)>;
    using input_t = base_t::input_ports_type;
    using output_t = base_t::output_ports_type;

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
                   std::vector<phlex::experimental::identifier> const& layer_names,
                   phlex::experimental::product_specifications output,
                   result_initializer_t result_initializer) :
      base_t{g},
      result_repeater_{g,
                       node_name,
                       stage,
                       partition_layer_name,
                       std::move(output),
                       std::move(result_initializer)},
      support_{g,
               node_name,
               layer_names,
               named_index_port{partition_layer_name,
                                std::nullopt,
                                &result_repeater_.flush_port(),
                                &result_repeater_.index_port()}},
      join_{make_join(g, std::make_index_sequence<NInputs>{})}
    {
      assert(NInputs == layer_names.size());

      make_edge(tbb::flow::output_port<1>(result_repeater_), input_port<0>(join_));
      auto set_ports = [this]<std::size_t... Is>(std::index_sequence<Is...>) {
        this->set_external_ports(
          input_t{result_repeater_.partition_port(), support_.repeater(Is).data_port()...},
          output_t{join_});
        // Connect repeaters to join
        (make_edge(support_.repeater(Is), input_port<Is + 1>(join_)), ...);
        // Logical data index i maps to external port i + 1, excluding the partition port.
        receivers_ = {&input_port<Is + 1>(*this)...};
      };

      set_ports(std::make_index_sequence<NInputs>{});
    }

    tbb::flow::receiver<index_message>& partition_port()
    {
      return result_repeater_.partition_port();
    }
    tbb::flow::sender<message>& output_port()
    {
      return tbb::flow::output_port<0>(result_repeater_);
    }
    tbb::flow::receiver<std::size_t>& notify_result_repeater_port()
    {
      return input_port<3>(result_repeater_);
    }

    // Returns one named_index_port per slot the index router must wire up.
    //
    // The first entry is the *partition slot* — it routes the partition index to the
    // accumulator's index_port and delivers the partition-level flush token to the
    // accumulator's flush_port.  Its routing layer is the partition layer (e.g. "job"),
    // but its counting layer is the fold's most-derived input data layer (e.g. "event"),
    // because the accumulator is incremented once per fold call (= once per cell at the
    // most-derived input layer) and the flush count must balance against that.
    //
    // The remaining entries are the data-input slots, one per repeater.  For these the
    // routing layer and counting layer are the same: the repeater both routes and counts
    // at its own input layer.
    //
    // FIXME: For multi-input folds, the "most-derived" input layer is the deepest of
    // `layer_names` in the data hierarchy.  Until the router exposes a depth comparison, we
    // pick `layer_names[0]`; all current tests have single-input folds so this is correct in
    // practice.
    std::vector<named_index_port> index_ports() { return support_.index_ports(); }

    tbb::flow::receiver<message>& receiver(std::size_t index)
    {
      return phlex::detail::receiver_for(receivers_, index);
    }

  private:
    internal::accumulator_node<FoldResult> result_repeater_;
    // Destroy the join before its data repeaters, then the typed accumulator.
    internal::join_support support_;
    tbb::flow::join_node<join_args_t, tbb::flow::tag_matching> join_;
    std::vector<tbb::flow::receiver<message>*> receivers_;
  };

  // Uses cached data receivers, excluding the partition port, for runtime lookup.
  template <typename FoldResult, std::size_t N>
  tbb::flow::receiver<message>& receiver_for(fold_join_node<FoldResult, N>& join,
                                             std::size_t const index)
  {
    return join.receiver(index);
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
      return join.receiver(index);
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
