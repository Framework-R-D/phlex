#include "phlex/core/edge_maker.hpp"

#include "spdlog/spdlog.h"
#include "tbb/flow_graph.h"

#include <cassert>

namespace phlex::experimental {
  multiplexer::head_ports_t make_provider_edges(multiplexer::head_ports_t head_ports,
                                                declared_providers& providers)
  {
    assert(!head_ports.empty());

    multiplexer::head_ports_t result;
    for (auto const& [node_name, ports] : head_ports) {
      for (auto const& port : ports) {
        // Find the provider that has the right product name (hidden in the
        // output port) and the right family (hidden in the input port).
        bool found_match = false;
        for (auto const& [_, p] : providers) {
          auto& provider = *p;
          // FIXME: The check should probably be more robust.  Right now, the
          //        product_specification buried in the provider.input()[0] call does not
          //        have its type set, which prevents us from doing a simpler comparison
          //        (e.g., port.product_label == provider.input()[0]).
          if (port.product_label.name.full() == provider.input()[0].name.full() &&
              port.product_label.layer == provider.input()[0].layer) {
            assert(provider.ports().size() == 1);
            auto it = result.find(provider.full_name());
            if (it == result.cend()) {
              multiplexer::named_input_ports_t provider_input_ports;
              provider_input_ports.emplace_back(port.product_label, provider.ports()[0]);
              result.try_emplace(provider.full_name(), std::move(provider_input_ports));
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
    return result;
  }
}
