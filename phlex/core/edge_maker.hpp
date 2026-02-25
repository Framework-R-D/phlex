#ifndef PHLEX_CORE_EDGE_MAKER_HPP
#define PHLEX_CORE_EDGE_MAKER_HPP

#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_provider.hpp"
#include "phlex/core/edge_creation_policy.hpp"
#include "phlex/core/filter.hpp"
#include "phlex/core/index_router.hpp"

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

  index_router::provider_input_ports_t make_provider_edges(index_router::head_ports_t head_ports,
                                                           declared_providers& providers);

  class edge_maker {
  public:
    template <typename... Args>
    edge_maker(Args&... args);

    template <typename... Args>
    void operator()(tbb::flow::graph& g,
                    index_router& multi,
                    std::map<std::string, filter>& filters,
                    declared_outputs& outputs,
                    declared_providers& providers,
                    Args&... consumers);

  private:
    template <typename T>
    index_router::head_ports_t edges(std::map<std::string, filter>& filters, T& consumers);

    template <typename T>
    std::map<std::string, named_index_ports> multilayer_ports(T& consumers);

    edge_creation_policy producers_;
  };

  // =============================================================================
  // Implementation
  template <typename... Args>
  edge_maker::edge_maker(Args&... producers) : producers_{producers...}
  {
  }

  template <typename T>
  index_router::head_ports_t edge_maker::edges(std::map<std::string, filter>& filters, T& consumers)
  {
    index_router::head_ports_t result;
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

        make_edge(*producer->output_port, *receiver_port);
      }
    }
    return result;
  }

  template <typename T>
  std::map<std::string, named_index_ports> edge_maker::multilayer_ports(T& consumers)
  {
    // Folds are not yet supported with the new caching system
    if constexpr (std::same_as<T, declared_folds>) {
      return {};
    } else {
      std::map<std::string, named_index_ports> result;
      for (auto& [node_name, node] : consumers) {
        if (auto const& ports = node->index_ports(); not ports.empty()) {
          result.try_emplace(node_name, ports);
        }
      }
      return result;
    }
  }

  template <typename... Args>
  void edge_maker::operator()(tbb::flow::graph& g,
                              index_router& multi,
                              std::map<std::string, filter>& filters,
                              declared_outputs& outputs,
                              declared_providers& providers,
                              Args&... consumers)
  {
    // Create edges to outputs
    for (auto const& [output_name, output_node] : outputs) {
      for (auto& [_, provider] : providers) {
        make_edge(provider->output_port(), output_node->port());
      }
      for (auto const& named_port : producers_.values()) {
        make_edge(*named_port.output_port, output_node->port());
      }
    }

    // Create normal edges
    index_router::head_ports_t head_ports;
    (head_ports.merge(edges(filters, consumers)), ...);
    // Eventually, we want to look at the filled-in head_ports and
    // figure out what provider nodes are needed.
    // For now, we take as input a mapping of declared_providers.

    if (head_ports.empty()) {
      // This can happen for jobs that only execute the driver, which is helpful for debugging
      return;
    }

    auto provider_input_ports = make_provider_edges(std::move(head_ports), providers);

    std::map<std::string, named_index_ports> multilayer_join_index_ports;
    (multilayer_join_index_ports.merge(multilayer_ports(consumers)), ...);

    multi.finalize(g, std::move(provider_input_ports), std::move(multilayer_join_index_ports));
  }
}

#endif // PHLEX_CORE_EDGE_MAKER_HPP
