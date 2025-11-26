#ifndef PHLEX_CORE_EDGE_MAKER_HPP
#define PHLEX_CORE_EDGE_MAKER_HPP

#include "phlex/core/declared_output.hpp"
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

  using product_name_t = std::string;

  class edge_maker {
  public:
    template <typename... Args>
    edge_maker(Args&... args);

    template <typename... Args>
    void operator()(tbb::flow::input_node<message>& source,
                    multiplexer& multi,
                    std::map<std::string, filter>& filters,
                    declared_outputs& outputs,
                    Args&... consumers);

  private:
    template <typename T>
    multiplexer::head_ports_t edges(std::map<std::string, filter>& filters, T& consumers);

    edge_creation_policy producers_;
  };

  // =============================================================================
  // Implementation
  template <typename... Args>
  edge_maker::edge_maker(Args&... producers) : producers_{producers...}
  {
  }

  template <typename T>
  multiplexer::head_ports_t edge_maker::edges(std::map<std::string, filter>& filters, T& consumers)
  {
    multiplexer::head_ports_t result;
    for (auto& [node_name, node] : consumers) {
      tbb::flow::receiver<message>* collector = nullptr;
      if (auto coll_it = filters.find(node_name); coll_it != cend(filters)) {
        collector = &coll_it->second.data_port();
      }

      for (auto const& query : node->input()) {
        auto* receiver_port = collector ? collector : &node->port(query);
        auto producer = producers_.find_producer(query);
        if (not producer) {
          // Is there a way to detect mis-specified product dependencies?
          result[node_name].push_back({query, receiver_port});
          continue;
        }

        make_edge(*producer->port, *receiver_port);
      }
    }
    return result;
  }

  template <typename... Args>
  void edge_maker::operator()(tbb::flow::input_node<message>& source,
                              multiplexer& multi,
                              std::map<std::string, filter>& filters,
                              declared_outputs& outputs,
                              Args&... consumers)
  {
    make_edge(source, multi);

    // Create edges to outputs
    for (auto const& [output_name, output_node] : outputs) {
      make_edge(source, output_node->port());
      for (auto const& named_port : producers_.values()) {
        make_edge(*named_port.to_output, output_node->port());
      }
    }

    // Create normal edges
    multiplexer::head_ports_t head_ports;
    (head_ports.merge(edges(filters, consumers)), ...);

    multi.finalize(std::move(head_ports));
  }
}

#endif // PHLEX_CORE_EDGE_MAKER_HPP
