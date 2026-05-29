#ifndef PHLEX_CORE_PRODUCER_CATALOG_HPP
#define PHLEX_CORE_PRODUCER_CATALOG_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/message.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <map>
#include <ranges>
#include <string>

namespace phlex::experimental {
  using product_suffix_t = identifier;

  class PHLEX_CORE_EXPORT producer_catalog {
  public:
    template <typename... Args>
    explicit producer_catalog(Args&... producers);

    struct named_output_port {
      algorithm_name node;
      tbb::flow::sender<message>* output_port;
      type_id type;
    };

    named_output_port const* find_producer(product_selector const& query,
                                           algorithm_name const& consumer_name) const;
    auto values() const { return producers_ | std::views::values; }

  private:
    template <typename T>
    static std::multimap<product_suffix_t, named_output_port> producing_nodes(T& nodes);

    std::multimap<product_suffix_t, named_output_port> producers_;
  };

  // =============================================================================
  // Implementation
  template <typename T>
  std::multimap<product_suffix_t, producer_catalog::named_output_port>
  producer_catalog::producing_nodes(T& nodes)
  {
    std::multimap<product_suffix_t, named_output_port> result;
    for (auto const& [node_name, node] : nodes) {
      for (auto const& product_spec : node->output()) {
        result.emplace(product_spec.suffix(),
                       named_output_port{node_name, &node->output_port(), product_spec.type()});
      }
    }
    return result;
  }

  template <typename... Args>
  producer_catalog::producer_catalog(Args&... producers)
  {
    (producers_.merge(producing_nodes(producers)), ...);
  }
}

#endif // PHLEX_CORE_PRODUCER_CATALOG_HPP
