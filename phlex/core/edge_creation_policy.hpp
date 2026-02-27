#ifndef PHLEX_CORE_EDGE_CREATION_POLICY_HPP
#define PHLEX_CORE_EDGE_CREATION_POLICY_HPP

#include "phlex/core/message.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <map>
#include <ranges>
#include <string>

namespace phlex::experimental {
  using product_name_t = std::string;

  class edge_creation_policy {
  public:
    template <typename... Args>
    edge_creation_policy(Args&... producers);

    struct named_output_port {
      algorithm_name node;
      tbb::flow::sender<message>* output_port;
      type_id type;
    };

    named_output_port const* find_producer(product_query const& query) const;
    auto values() const { return producers_ | std::views::values; }

  private:
    template <typename T>
    static std::multimap<product_name_t, named_output_port> producing_nodes(T& nodes);

    std::multimap<product_name_t, named_output_port> producers_;
  };

  // =============================================================================
  // Implementation
  template <typename T>
  std::multimap<product_name_t, edge_creation_policy::named_output_port>
  edge_creation_policy::producing_nodes(T& nodes)
  {
    std::multimap<product_name_t, named_output_port> result;
    for (auto const& [node_name, node] : nodes) {
      for (auto const& product_name : node->output()) {
        if (empty(product_name.name()))
          continue;

        result.emplace(product_name.name(),
                       named_output_port{node_name, &node->output_port(), product_name.type()});
      }
    }
    return result;
  }

  template <typename... Args>
  edge_creation_policy::edge_creation_policy(Args&... producers)
  {
    (producers_.merge(producing_nodes(producers)), ...);
  }
}

#endif // PHLEX_CORE_EDGE_CREATION_POLICY_HPP
