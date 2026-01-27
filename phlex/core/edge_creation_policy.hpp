#ifndef PHLEX_CORE_EDGE_CREATION_POLICY_HPP
#define PHLEX_CORE_EDGE_CREATION_POLICY_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <map>
#include <ranges>
#include <spdlog/spdlog.h>
#include <string>

namespace phlex::experimental {
  using product_name_t = std::string;

  class edge_creation_policy {
  public:
    template <typename... Args>
    edge_creation_policy(Args&... producers);

    struct named_output_port {
      std::string suffix;
      algorithm_name creator;
      tbb::flow::sender<message>* port;
      tbb::flow::sender<message>* to_output;
      type_id type;
    };

    named_output_port const* find_producer(product_query const& query) const;
    auto values() const { return producers_; }

  private:
    // Store a stack of all named_output_ports
    std::forward_list<named_output_port> producers_;

    // And maps indexing by (hash of) each field
    std::multimap<std::uint64_t, named_output_port const*> creator_db_;
    std::multimap<std::uint64_t, named_output_port const*> suffix_db_;
    std::multimap<std::uint64_t, named_output_port const*> type_db_;

    // Utility to add producers
    template <typename T>
    void add_nodes(T& nodes);
    static std::uint64_t hash_string(std::string const& str);
  };

  // =============================================================================
  // Implementation

  template <typename T>
  void edge_creation_policy::add_nodes(T& nodes)
  {
    for (auto const& [node_name, node] : nodes) {
      for (auto const& spec : node->output()) {
        spdlog::debug("Adding product {} with type {}", spec.full(), spec.type());
        auto& port = producers_.emplace_front(
          spec.name(), spec.qualifier(), &node->sender(), &node->to_output(), spec.type());

        // creator_db_ contains entries for plugin name and algorithm name
        creator_db_.emplace(hash_string(spec.plugin()), &port);
        creator_db_.emplace(hash_string(spec.algorithm()), &port);

        suffix_db_.emplace(hash_string(spec.name()), &port);
        type_db_.emplace(spec.type().hash(), &port);
      }
    }
  }

  template <typename... Args>
  edge_creation_policy::edge_creation_policy(Args&... producers)
  {
    (add_nodes(producers), ...);
  }
}

#endif // PHLEX_CORE_EDGE_CREATION_POLICY_HPP
