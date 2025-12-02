#ifndef PHLEX_CORE_EDGE_MAKER_HPP
#define PHLEX_CORE_EDGE_MAKER_HPP

#include "phlex/core/declared_output.hpp"
#include "phlex/core/edge_creation_policy.hpp"
#include "phlex/core/filter.hpp"
#include "phlex/core/multiplexer.hpp"

#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace phlex::experimental {
  using namespace std::string_literals;

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
                    declared_providers& providers,
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
                              declared_providers& providers,
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
    // Eventually, we want to look at the filled-in head_ports and
    // figure out what provider nodes are needed.
    // For now, we take as input a mapping of declared_providers.

    // wire up the head_ports to the given providers.
    // If any head_port does not have a matching provider, that is
    // an error.
    multiplexer::head_ports_t provider_ports;
    assert(!head_ports.empty());

    for (auto const& [node_name, ports] : head_ports) {
      for (auto const& port : ports) {
        // Find the provider that has the right product name (hidden in the
        // output port) and the right family (hidden in the input port).
        bool found_match = false;
        for (auto const& [_, p] : providers) {
          // FIXME: The check should probably be more robust.  Right now, the product_specification
          //        buried in the p->input()[0] call does not have its type set, which prevents us from
          //        doing a simpler comparison (e.g., port.product_label == p->input()[0]).
          if (port.product_label.name.full() == p->input()[0].name.full()) {
            auto& provider = *p;
            assert(provider.ports().size() == 1);
            auto it = provider_ports.find(provider.full_name());
            if (it == provider_ports.cend()) {
              multiplexer::named_input_ports_t provider_input_ports;
              provider_input_ports.emplace_back(port.product_label, provider.ports()[0]);
              provider_ports.try_emplace(provider.full_name(), std::move(provider_input_ports));
            }
            spdlog::info("Connecting provider {} to node {} (product: {})",
                         provider.full_name(),
                         node_name,
                         port.product_label.to_string());
            make_edge(provider.sender(), *(port.port));
            found_match = true;
            break;
          }
        }
        if (!found_match) {
          throw std::runtime_error("No provider found for product: "s +
                                   port.product_label.to_string());
        }
      }
    }

    // We must have at least one provider port, or there can be no data to
    // process.
    assert(!provider_ports.empty());
    // Pass the providers to the multiplexer.
    multi.finalize(std::move(provider_ports));
  }
}

#endif // PHLEX_CORE_EDGE_MAKER_HPP
