#ifndef PHLEX_CORE_EDGE_MAKER_HPP
#define PHLEX_CORE_EDGE_MAKER_HPP

#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/dot/attributes.hpp"
#include "phlex/core/dot/data_graph.hpp"
#include "phlex/core/dot/function_graph.hpp"
#include "phlex/core/edge_creation_policy.hpp"
#include "phlex/core/filter.hpp"
#include "phlex/core/multiplexer.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace phlex::experimental {

  template <typename T>
  concept supports_output = requires(T t) {
    { t->output() };
  };

  template <typename T>
  struct consumers {
    T& data;
    dot::attributes attributes;
  };

  template <typename T>
  consumers(T&) -> consumers<T>;

  template <typename T>
  consumers(T&, dot::attributes) -> consumers<T>;

  using product_name_t = std::string;

  class edge_maker {
  public:
    template <typename... Args>
    edge_maker(std::string const& file_prefix, Args&... args);

    template <typename... Args>
    void operator()(tbb::flow::input_node<message>& source,
                    multiplexer& multi,
                    std::map<std::string, filter>& filters,
                    declared_outputs& outputs,
                    consumers<Args>... cons);

    auto release_data_graph() { return std::move(data_graph_); }
    auto release_function_graph() { return std::move(function_graph_); }

  private:
    template <typename T>
    void record_attributes(T& consumers);

    template <typename T>
    multiplexer::head_ports_t edges(std::map<std::string, filter>& filters, T& consumers);

    std::unique_ptr<dot::function_graph> function_graph_;
    std::unique_ptr<dot::data_graph> data_graph_;

    edge_creation_policy producers_;
    std::map<std::string, dot::attributes> attributes_;

    template <typename T>
    void make_the_node(T& node, dot::attributes const& node_attributes)
    {
      if (not function_graph_) {
        return;
      }

      auto const& node_name = node->full_name();
      function_graph_->node(node_name, node_attributes);
      for (auto const& predicate_name : node->when()) {
        function_graph_->edge(predicate_name, node_name, {.color = "red"});
      }

      if constexpr (supports_output<decltype(node)>) {
        if (data_graph_) {
          data_graph_->add(node_name, node->input(), node->output());
        }
      }
    }

    template <typename Sender, typename Receiver>
    void make_the_edge(Sender& sender,
                       Receiver& receiver,
                       std::string const& receiver_node_name,
                       std::string const& product_name)
    {
      make_edge(*sender.port, receiver);
      if (function_graph_) {
        function_graph_->edge(sender.node.full(),
                              receiver_node_name,
                              {.color = "blue",
                               .fontsize = dot::default_fontsize,
                               .label = dot::parenthesized(product_name)});
      }
    }
  };

  // =============================================================================
  // Implementation
  template <typename... Args>
  edge_maker::edge_maker(std::string const& file_prefix, Args&... producers) :
    function_graph_{file_prefix.empty() ? nullptr : std::make_unique<dot::function_graph>()},
    data_graph_{file_prefix.empty() ? nullptr : std::make_unique<dot::data_graph>()},
    producers_{producers...}
  {
  }

  template <typename T>
  void edge_maker::record_attributes(T& consumers)
  {
    auto const& [data, attributes] = consumers;
    for (auto const& node_name : data | std::views::keys) {
      attributes_[node_name] = attributes;
    }
  }

  template <typename T>
  multiplexer::head_ports_t edge_maker::edges(std::map<std::string, filter>& filters, T& consumers)
  {
    multiplexer::head_ports_t result;
    auto const& [data, attributes] = consumers;
    for (auto& [node_name, node] : data) {
      tbb::flow::receiver<message>* collector = nullptr;
      if (auto coll_it = filters.find(node_name); coll_it != cend(filters)) {
        collector = &coll_it->second.data_port();
      }

      make_the_node(node, attributes);

      for (auto const& product_label : node->input()) {
        auto* receiver_port = collector ? collector : &node->port(product_label);
        auto producer = producers_.find_producer(product_label.name);
        if (not producer) {
          // Is there a way to detect mis-specified product dependencies?
          result[node_name].push_back({product_label, receiver_port});
          continue;
        }

        make_the_edge(*producer, *receiver_port, node_name, to_name(product_label));
      }
    }
    return result;
  }

  template <typename... Args>
  void edge_maker::operator()(tbb::flow::input_node<message>& source,
                              multiplexer& multi,
                              std::map<std::string, filter>& filters,
                              declared_outputs& outputs,
                              consumers<Args>... cons)
  {
    (record_attributes(cons), ...);

    make_edge(source, multi);

    // Create edges to outputs
    auto& unfolds = std::get<consumers<declared_unfolds>&>(std::tie(cons...));

    for (auto const& [output_name, output_node] : outputs) {
      make_edge(source, output_node->port());
      if (function_graph_) {
        function_graph_->node(output_name, {.shape = "cylinder"});
        function_graph_->edge("Source", output_name, {.color = "gray"});
      }
      for (auto const& named_port : producers_.values()) {
        make_edge(*named_port.to_output, output_node->port());
        if (function_graph_) {
          function_graph_->edge(named_port.node.full(), output_name, {.color = "gray"});
        }
      }
      for (auto const& [unfold_name, unfold] : unfolds.data) {
        make_edge(unfold->to_output(), output_node->port());
        if (function_graph_) {
          function_graph_->edge(unfold_name, output_name, {.color = "gray"});
        }
      }
    }

    // Create normal edges
    multiplexer::head_ports_t head_ports;
    (head_ports.merge(edges(filters, cons)), ...);

    // Create head nodes for unfolds
    std::set<std::string> remove_ports_for_products;
    for (auto const& [name, unfold] : unfolds.data) {
      multiplexer::head_ports_t heads;
      for (auto const& product_name : unfold->output()) {
        // There can be multiple head nodes that require the same product.
        remove_ports_for_products.insert(product_name.full());
        for (auto const& [node_name, ports] : head_ports) {
          for (auto const& port : ports) {
            if (to_name(port.product_label) != product_name.name()) {
              continue;
            }
            heads[node_name].push_back(port);
          }
        }
      }
      unfold->finalize(std::move(heads));
    }

    // Remove head nodes claimed by unfolds
    for (auto const& key : remove_ports_for_products) {
      for (auto& ports : head_ports | std::views::values) {
        std::erase_if(ports,
                      [&key](auto const& port) { return to_name(port.product_label) == key; });
      }
    }

    multi.finalize(std::move(head_ports));

    if (function_graph_) {
      for (auto const& [name, unfold] : unfolds.data) {
        for (auto const& [node_name, ports] : unfold->downstream_ports()) {
          function_graph_->edges_for(name, node_name, ports);
        }
      }

      for (auto const& [node_name, ports] : multi.downstream_ports()) {
        function_graph_->edges_for("Source", node_name, ports);
      }
    }
  }
}

#endif // PHLEX_CORE_EDGE_MAKER_HPP
