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
      return tbb::flow::join_node<args_t, tbb::flow::tag_matching>{
        g, type_t<message_matcher, Is>{}...};
    }

  public:
    multilayer_join_node(tbb::flow::graph& g,
                         std::string const& node_name,
                         std::vector<std::string> layer_names) :
      base_t{g},
      join_{make_join(g, std::make_index_sequence<n_inputs>{})},
      name_{node_name},
      layers_{std::move(layer_names)}
    {
      assert(n_inputs == layers_.size());

      // Removes duplicates
      std::set collapsed_layers{layers_.begin(), layers_.end()};

      // Add repeaters only if there are non-duplicate layer specifications
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

    std::vector<named_index_port> index_ports()
    {
      // Returns an empty list if no repeaters are required
      std::vector<named_index_port> result;
      result.reserve(repeaters_.size());
      for (std::size_t i = 0; i != n_inputs; ++i) {
        result.emplace_back(layers_[i], &repeaters_[i]->flush_port(), &repeaters_[i]->index_port());
      }
      return result;
    }

  private:
    std::vector<std::unique_ptr<detail::repeater_node>> repeaters_;
    tbb::flow::join_node<args_t, tbb::flow::tag_matching> join_;
    std::string const name_;
    std::vector<std::string> const layers_;
  };

  namespace detail {
    // Stateless placeholder for cases where no join is needed
    struct no_join {
      named_index_ports index_ports() const { return {}; }
    };

    template <std::size_t N>
    struct pre_node {
      using type = multilayer_join_node<N>;
    };

    template <>
    struct pre_node<1ull> {
      using type = no_join;
    };
  }

  template <std::size_t N>
  using join_or_none_t = typename detail::pre_node<N>::type;

  template <std::size_t N>
  join_or_none_t<N> make_join_or_none(tbb::flow::graph& g,
                                      std::string const& node_name,
                                      std::vector<std::string> const& layers)
  {
    if constexpr (N > 1ull) {
      return multilayer_join_node<N>{g, node_name, layers};
    } else {
      return detail::no_join{};
    }
  }

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
    template <std::size_t N>
    std::vector<tbb::flow::receiver<message>*> input_ports(join_or_none_t<N>& join)
    {
      static_assert(N > 1ull, "input_ports should not be called for N=1");
      return [&join]<std::size_t... Is>(
               std::index_sequence<Is...>) -> std::vector<tbb::flow::receiver<message>*> {
        return {&input_port<Is>(join)...};
      }(std::make_index_sequence<N>{});
    }

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

  template <std::size_t N, typename Node>
  std::vector<tbb::flow::receiver<message>*> input_ports(join_or_none_t<N>& join, Node& node)
  {
    if constexpr (N == 1ull) {
      return {&node};
    } else {
      return detail::input_ports<N>(join);
    }
  }

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
