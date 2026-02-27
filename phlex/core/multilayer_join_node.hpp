#ifndef PHLEX_CORE_MULTILAYER_JOIN_NODE_HPP
#define PHLEX_CORE_MULTILAYER_JOIN_NODE_HPP

#include "phlex/core/detail/repeater_node.hpp"
#include "phlex/core/message.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <cassert>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  template <typename Input>
  using multilayer_join_node_base_t = tbb::flow::composite_node<Input, std::tuple<Input>>;

  // A multilayer_join_node is a TBB composite flow-graph node that collects one message
  // from each of its N input streams and forwards the complete tuple downstream once all
  // N messages for the same data unit have arrived.
  //
  // Each input stream is associated with a named hierarchy layer.  When two or more
  // inputs belong to *different* layers a repeater_node is inserted in front of each
  // stream so that a product originating at a parent layer is repeated for every
  // child-layer data unit, allowing the underlying join_node to see a matching message on
  // every port.  When all inputs share the same layer, repeaters are unnecessary and the
  // join_node is used directly.
  //
  // Schematic with N inputs spanning multiple distinct layers:
  //
  //      data[0] ───┐
  //     flush[0] ───┤ repeater[0] ──┐
  //     index[0] ───┘               │
  //                ⋮                ├──► join_node ──► message tuple
  //    data[N-1] ───┐               │
  //   flush[N-1] ───┤ repeater[N-1] ┘
  //   index[N-1] ───┘
  //
  // When all inputs share the same layer the repeaters are omitted:
  //
  //     data[0] ──┐
  //               ├──► join_node ──► message tuple
  //   data[N-1] ──┘

  template <std::size_t n_inputs>
    requires(n_inputs > 1)
  class multilayer_join_node : public multilayer_join_node_base_t<message_tuple<n_inputs>> {
    using base_t = multilayer_join_node_base_t<message_tuple<n_inputs>>;
    using input_t = typename base_t::input_ports_type;
    using output_t = typename base_t::output_ports_type;

    using args_t = message_tuple<n_inputs>;

    template <std::size_t... Is>
    static auto make_join(tbb::flow::graph& g, std::index_sequence<Is...>)
    {
      // tag_matching causes TBB to group messages by the value returned by
      // message_matcher, which extracts the message ID.  Messages with the same ID across
      // all ports are forwarded together as one tuple, ensuring that each output contains
      // exactly the messages that belong to the same data unit.
      return tbb::flow::join_node<args_t, tbb::flow::tag_matching>{
        g, type_t<message_matcher, Is>{}...};
    }

  public:
    multilayer_join_node(tbb::flow::graph& g,
                         std::string const& node_name,
                         std::vector<identifier> layer_names) :
      base_t{g},
      join_{make_join(g, std::make_index_sequence<n_inputs>{})},
      name_{node_name},
      layers_{std::move(layer_names)}
    {
      assert(n_inputs == layers_.size());

      // Collapse to the set of distinct layer names.  More than one distinct layer means
      // at least one input crosses a layer boundary and therefore every input stream
      // needs a repeater_node.
      std::set collapsed_layers{layers_.begin(), layers_.end()};

      // Add repeaters only if the inputs span more than one distinct layer.
      if (collapsed_layers.size() > 1) {
        repeaters_.reserve(n_inputs);
        for (auto const& layer : layers_) {
          repeaters_.push_back(std::make_unique<detail::repeater_node>(g, name_, layer));
        }
      }

      auto set_ports = [this]<std::size_t... Is>(std::index_sequence<Is...>) {
        if (repeaters_.empty()) {
          // No repeating behavior necessary if all specified layer names are the same
          // Just use TBB's join_node.
          this->set_external_ports(input_t{input_port<Is>(join_)...}, output_t{join_});
        } else {
          this->set_external_ports(input_t{repeaters_[Is]->data_port()...}, output_t{join_});
          // Connect repeaters to join
          (make_edge(*repeaters_[Is], input_port<Is>(join_)), ...);
        }
      };

      set_ports(std::make_index_sequence<n_inputs>{});
    }

    // Returns one named_index_port per repeater so that the index router can deliver
    // flush and index messages to each repeater.  Returns an empty list when no repeaters
    // were constructed (all inputs share the same layer).
    std::vector<named_index_port> index_ports()
    {
      std::vector<named_index_port> result;
      result.reserve(repeaters_.size());
      for (std::size_t i = 0; i != repeaters_.size(); ++i) {
        result.emplace_back(layers_[i], &repeaters_[i]->flush_port(), &repeaters_[i]->index_port());
      }
      return result;
    }

  private:
    std::vector<std::unique_ptr<detail::repeater_node>> repeaters_;
    tbb::flow::join_node<args_t, tbb::flow::tag_matching> join_;
    std::string const name_;
    std::vector<identifier> const layers_;
  };

  namespace detail {
    // Stateless placeholder used instead of multilayer_join_node when a node has only a
    // single input (no joining is required).
    struct no_join {
      named_index_ports index_ports() const { return {}; }
    };

    // Maps the number of inputs to the appropriate join type: a real multilayer_join_node
    // for N > 1, or the no_join sentinel for N == 1.
    template <std::size_t N>
    struct pre_node {
      using type = multilayer_join_node<N>;
    };

    template <>
    struct pre_node<1ull> {
      using type = no_join;
    };
  }

  // Resolves to multilayer_join_node<N> for N > 1 and to no_join for N == 1.
  template <std::size_t N>
  using join_or_none_t = typename detail::pre_node<N>::type;

  // Constructs a multilayer_join_node when N > 1, or a no_join when N == 1.
  template <std::size_t N>
  join_or_none_t<N> make_join_or_none(tbb::flow::graph& g,
                                      std::string const& node_name,
                                      std::vector<identifier> const& layers)
  {
    if constexpr (N > 1ull) {
      return multilayer_join_node<N>{g, node_name, layers};
    } else {
      return detail::no_join{};
    }
  }

  // Translates a runtime port index into a reference to the corresponding compile-time
  // input port by recursively incrementing the compile-time parameter I until it matches
  // the runtime index.
  template <std::size_t I, std::size_t N>
  tbb::flow::receiver<message>& receiver_for(multilayer_join_node<N>& join, std::size_t const index)
  {
    if constexpr (I < N) {
      if (I != index) {
        return receiver_for<I + 1ull, N>(join, index);
      }
      return input_port<I>(join);
    }
    throw std::runtime_error("Should never get here");
  }

  namespace detail {
    // Returns pointers to all N input ports of the join node.  Only valid for N > 1;
    // callers with a single input should use the node directly.
    template <std::size_t N>
    std::vector<tbb::flow::receiver<message>*> input_ports(join_or_none_t<N>& join)
    {
      static_assert(N > 1ull, "input_ports should not be called for N=1");
      return [&join]<std::size_t... Is>(
               std::index_sequence<Is...>) -> std::vector<tbb::flow::receiver<message>*> {
        return {&input_port<Is>(join)...};
      }(std::make_index_sequence<N>{});
    }

    // Looks up the port index for the given product label, then returns a reference to
    // the corresponding input port of the join node.  Only valid for N > 1.
    template <std::size_t N>
    tbb::flow::receiver<message>& receiver_for(join_or_none_t<N>& join,
                                               product_queries const& product_labels,
                                               product_query const& product_label)
    {
      static_assert(N > 1ull, "receiver_for should not be called for N=1");
      auto const index = port_index_for(product_labels, product_label);
      return receiver_for<0ull, N>(join, index);
    }
  }

  // Returns all input-port pointers for a node.  For N == 1 the node itself is the sole
  // receiver; for N > 1 each port of the join is returned.
  template <std::size_t N, typename Node>
  std::vector<tbb::flow::receiver<message>*> input_ports(join_or_none_t<N>& join, Node& node)
  {
    if constexpr (N == 1ull) {
      return {&node};
    } else {
      return detail::input_ports<N>(join);
    }
  }

  // Returns the receiver for the input port that corresponds to the given product label.  For
  // N == 1 there is only one port so the node itself is returned; for N > 1 the port is looked
  // up by label within the join.
  template <std::size_t N, typename Node>
  tbb::flow::receiver<message>& receiver_for(join_or_none_t<N>& join,
                                             product_queries const& product_labels,
                                             product_query const& product_label,
                                             Node& node)
  {
    if constexpr (N == 1ull) {
      return node;
    } else {
      return detail::receiver_for<N>(join, product_labels, product_label);
    }
  }
}

#endif // PHLEX_CORE_MULTILAYER_JOIN_NODE_HPP
