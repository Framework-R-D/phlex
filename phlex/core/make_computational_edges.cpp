#include "phlex/core/make_computational_edges.hpp"

#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <span>

using namespace std::string_literals;

namespace phlex::experimental {
  namespace {
    provider_node* find_match(provider_nodes& providers, product_selector const& input_product)
    {
      auto pred = [&input_product](auto const& p) {
        return input_product.match(p->output_product(), p->layer(), p->stage());
      };
      auto proj = [](auto const& pair) -> provider_node* { return pair.second.get(); };

      if (auto it = std::ranges::find_if(providers, pred, proj); it != providers.end()) {
        return it->second.get();
      }
      return nullptr;
    }

    index_router::provider_input_ports_t edges_from_explicit_providers(
      index_router::head_ports_t head_ports, provider_nodes& explicit_providers)
    {
      assert(!head_ports.empty());

      // FIXME: Should return a list of head ports that cannot be matched to an explicit provider.

      index_router::provider_input_ports_t result;
      for (auto const& [node_name, ports] : head_ports) {
        for (auto const& [input_product, port] : ports) {
          // Find the provider that has the right product name (hidden in the
          // output port) and the right family (hidden in the input port).
          if (auto* matched_provider = find_match(explicit_providers, input_product)) {
            auto const provider_name = matched_provider->name().to_string();
            result.try_emplace(provider_name, input_product, matched_provider->input_port());
            spdlog::debug("Connecting provider {} to node {} (product: {})",
                          provider_name,
                          node_name,
                          input_product.to_string());
            make_edge(matched_provider->output_port(), *port);
          } else {
            throw std::runtime_error("No provider found for product: "s +
                                     input_product.to_string());
          }
        }
      }
      return result;
    }

    index_router::head_ports_t edges_within_computational_graph(
      producer_catalog const& producers,
      std::map<std::string, filter>& filters,
      std::span<products_consumer* const> consumers)
    {
      index_router::head_ports_t result;
      for (auto* node : consumers) {
        auto const node_name = node->name().to_string();
        tbb::flow::receiver<message>* collector = nullptr;
        if (auto coll_it = filters.find(node_name); coll_it != cend(filters)) {
          collector = &coll_it->second.data_port();
        }

        for (auto const& query : node->input()) {
          auto* receiver_port = collector ? collector : &node->port(query);
          auto producer = producers.find_producer(query, node->name());
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

    std::map<std::string, named_index_ports> multilayer_ports(
      std::span<products_consumer* const> consumers)
    {
      std::map<std::string, named_index_ports> result;
      for (auto* node : consumers) {
        if (auto const& ports = node->index_ports(); not ports.empty()) {
          result.try_emplace(node->name().to_string(), ports);
        }
      }
      return result;
    }

    void edges_to_outputs(provider_nodes& providers,
                          producer_catalog const& producers,
                          declared_outputs& outputs)
    {
      for (auto const& [output_name, output_node] : outputs) {
        for (auto const& provider : providers | std::views::values) {
          make_edge(provider->output_port(), output_node->port());
        }
        for (auto const& named_port : producers.values()) {
          make_edge(*named_port.output_port, output_node->port());
        }
      }
    }
  }

  std::tuple<index_router::provider_input_ports_t, std::map<std::string, named_index_ports>>
  make_computational_edges(node_catalog& nodes, std::map<std::string, filter>& filters)
  {
    auto const producers = nodes.producers();
    auto const consumers = nodes.consumers();

    auto head_ports = edges_within_computational_graph(producers, filters, consumers);
    if (head_ports.empty()) {
      // This can happen for jobs that only execute the driver, which is helpful for debugging
      return {};
    }

    edges_to_outputs(nodes.providers, producers, nodes.outputs);

    auto provider_input_ports =
      edges_from_explicit_providers(std::move(head_ports), nodes.providers);
    auto multilayer_join_index_ports = multilayer_ports(consumers);

    return std::make_tuple(std::move(provider_input_ports), std::move(multilayer_join_index_ports));
  }
}
