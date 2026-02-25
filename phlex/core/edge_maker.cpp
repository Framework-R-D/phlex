#include "phlex/core/edge_maker.hpp"

#include "spdlog/spdlog.h"
#include "tbb/flow_graph.h"

#include <cassert>

namespace phlex::experimental {
  index_router::provider_input_ports_t make_provider_edges(index_router::head_ports_t head_ports,
                                                           declared_providers& providers)
  {
    assert(!head_ports.empty());

    index_router::provider_input_ports_t result;
    for (auto const& [node_name, ports] : head_ports) {
      for (auto const& port : ports) {
        // Find the provider that has the right product name (hidden in the
        // output port) and the right family (hidden in the input port).
        bool found_match = false;
        for (auto const& [_, p] : providers) {
          auto& provider = *p;
          if (port.product_label.match(provider.output_product())) {
            if (!result.contains(provider.full_name())) {
              result.try_emplace(provider.full_name(), port.product_label, provider.input_port());
            }
            spdlog::debug("Connecting provider {} to node {} (product: {})",
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
    return result;
  }
}
