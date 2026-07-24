#include "phlex/core/make_computational_edges.hpp"

#include "fmt/format.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <span>
#include <stdexcept>

using namespace std::string_literals;

namespace phlex::detail {
  namespace {
    provider_node* find_matching_provider(provider_nodes& providers,
                                          product_selector const& input_product)
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

    provider_bundles find_matching_implicit_providers(source_map const& sources,
                                                      product_selector const& input_product)
    {
      provider_bundles result;
      for (auto const& src : sources | std::views::values) {
        result.append_range(src->create_providers(input_product));
      }
      return result;
    }

    std::pair<index_router::provider_input_ports_t, index_router::head_ports_t>
    edges_from_explicit_providers(index_router::head_ports_t head_ports,
                                  provider_nodes& explicit_providers)
    {
      assert(!head_ports.empty());

      index_router::provider_input_ports_t provider_input_ports;
      index_router::head_ports_t unconsumed_head_ports;
      for (auto const& [node_name, ports] : head_ports) {
        for (auto const& [input_product, port] : ports) {
          // Find the provider that has the right product name (hidden in the
          // output port) and the right family (hidden in the input port).
          if (auto* matched_provider = find_matching_provider(explicit_providers, input_product)) {
            auto const provider_name = matched_provider->name().to_string();
            provider_input_ports.try_emplace(
              provider_name, input_product, matched_provider->input_port());
            spdlog::debug("Connecting provider {} to node {} (product: {})",
                          provider_name,
                          node_name,
                          input_product.to_string());
            make_edge(matched_provider->output_port(), *port);
          } else {
            unconsumed_head_ports[node_name].push_back({input_product, port});
          }
        }
      }
      return {std::move(provider_input_ports), std::move(unconsumed_head_ports)};
    }

    std::pair<index_router::provider_input_ports_t, index_router::head_ports_t>
    edges_from_implicit_providers(index_router::head_ports_t head_ports,
                                  provider_nodes& providers,
                                  source_map const& sources,
                                  tbb::flow::graph& g)
    {
      index_router::provider_input_ports_t provider_input_ports;
      index_router::head_ports_t unconsumed_head_ports;
      for (auto const& [node_name, ports] : head_ports) {
        for (auto const& [input_product, port] : ports) {
          auto existing_provider_it = std::ranges::find_if(
            provider_input_ports, [&input_product](auto const& provider_entry) {
              return provider_entry.second.input_product == input_product;
            });

          if (existing_provider_it != provider_input_ports.end()) {
            auto* provider = providers.get(existing_provider_it->first);
            assert(provider != nullptr);
            make_edge(provider->output_port(), *port);
            continue;
          }

          // If we have a source node that can produce this product, use it.
          auto bundles = find_matching_implicit_providers(sources, input_product);
          if (bundles.empty()) {
            unconsumed_head_ports[node_name].push_back({input_product, port});
            continue;
          }

          // For now we require only one implicit provider.  This will change in the future.
          if (bundles.size() > 1ull) {
            auto error_msg = fmt::format(
              "Multiple implicit providers found for product '{}', required by node '{}':\n",
              input_product.to_string(),
              node_name);
            throw std::runtime_error(error_msg);
          }

          auto& bundle = bundles[0];
          auto const& spec = bundle.spec;
          auto node =
            std::make_unique<provider_node>(spec.creator(),
                                            bundle.max_concurrency.value,
                                            g,
                                            std::move(bundle.provider_function),
                                            spec,
                                            phlex::experimental::identifier{bundle.layer},
                                            phlex::experimental::identifier{bundle.stage});
          auto const provider_name = node->name().to_string();
          auto [_, inserted] =
            provider_input_ports.try_emplace(provider_name, input_product, node->input_port());
          if (!inserted) {
            throw std::runtime_error(
              fmt::format("Failed to create implicit provider for product selector '{}'\n"
                          "Implicit providers not yet supported for creators that created multiple "
                          "data products",
                          input_product));
          }
          make_edge(node->output_port(), *port);
          providers.try_emplace(provider_name, std::move(node));
        }
      }
      return {std::move(provider_input_ports), std::move(unconsumed_head_ports)};
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
          auto const* producer = producers.find_producer(query, node->name());
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
  make_computational_edges(node_catalog& nodes,
                           std::map<std::string, filter>& filters,
                           tbb::flow::graph& g)
  {
    auto const producers = nodes.producers();
    auto const consumers = nodes.consumers();

    auto head_ports = edges_within_computational_graph(producers, filters, consumers);
    if (head_ports.empty()) {
      // This can happen for jobs that only execute the driver, which is helpful for debugging
      return {};
    }

    auto [explicit_provider_input_ports, unconsumed_head_ports] =
      edges_from_explicit_providers(std::move(head_ports), nodes.providers);

    auto [implicit_provider_input_ports, unmatched_head_ports] = edges_from_implicit_providers(
      std::move(unconsumed_head_ports), nodes.providers, nodes.sources, g);

    if (not unmatched_head_ports.empty()) {
      std::string error_msg{"No provider found for the following required products:\n"};
      for (auto const& [node_name, ports] : unmatched_head_ports) {
        for (auto const& [input_product, _] : ports) {
          error_msg += fmt::format(
            "  - Node '{}' requires product '{}'\n", node_name, input_product.to_string());
        }
      }
      throw std::runtime_error(error_msg);
    }

    // Make edges to outputs after both implicit and explicit providers have been registered.
    edges_to_outputs(nodes.providers, producers, nodes.outputs);

    // Combine implicit and explicit provider input ports.
    auto provider_input_ports = std::move(explicit_provider_input_ports);
    provider_input_ports.merge(std::move(implicit_provider_input_ports));

    auto multilayer_join_index_ports = multilayer_ports(consumers);

    return std::make_tuple(std::move(provider_input_ports), std::move(multilayer_join_index_ports));
  }
}
