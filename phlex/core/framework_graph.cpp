#include "phlex/core/framework_graph.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/make_computational_edges.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <format>
#include <iostream>

namespace phlex::experimental {
  namespace {
    template <typename T>
    auto internal_edges_for_predicates(oneapi::tbb::flow::graph& g,
                                       declared_predicates& all_predicates,
                                       T const& consumers)
    {
      std::map<std::string, filter> result;
      for (auto const& [name, consumer] : consumers) {
        auto const& predicates = consumer->when();
        if (empty(predicates)) {
          continue;
        }

        auto [it, success] = result.try_emplace(name, g, *consumer);
        for (auto const& predicate_name : predicates) {
          if (auto predicate = all_predicates.get(predicate_name)) {
            make_edge(predicate->sender(), it->second.predicate_port());
            continue;
          }
          throw std::runtime_error(std::format(
            "A non-existent filter with the name '{}' was specified for {}", predicate_name, name));
        }
      }
      return result;
    }

    index_router::fold_partition_ports_t fold_partition_ports(declared_folds const& folds)
    {
      index_router::fold_partition_ports_t result;
      for (auto& [fold_name, fold_node] : folds) {
        result.try_emplace(fold_name, fold_node->partition_layer(), &fold_node->partition_port());
      }
      return result;
    }

    // Collects (input layer, child layer) pairs and per-input-layer unfold counts from all
    // registered unfold nodes.  One pair is emitted per (unfold node, input layer): a
    // multi-input unfold contributes the same child layer under each of its input layers,
    // which is over-approximate but harmless — only the most-derived input actually parents
    // children at runtime, and the extra synthetic paths are never reached by
    // counting_layer_hashes_under for any real fold.  The per-input-layer count lets
    // flush_gates wait for a flush message from every unfold that consumes a given layer
    // before evaluating done().
    index_router::unfold_data unfold_layers(declared_unfolds const& unfolds)
    {
      index_router::unfold_data result;
      for (auto const& n : unfolds | std::views::values) {
        identifier const child_layer{n->child_layer()};
        for (auto const& input : n->input()) {
          auto const& input_layer = static_cast<identifier const&>(input.layer);
          if (input_layer.empty()) {
            continue;
          }
          ++result.count_per_input_layer[input_layer];
          result.layer_pairs.push_back({.input = input_layer, .output = child_layer});
        }
      }
      return result;
    }
  }

  framework_graph::framework_graph(int const max_parallelism) :
    framework_graph{[](framework_driver& driver) { driver.yield(data_cell_index::job()); },
                    max_parallelism}
  {
  }

  framework_graph::framework_graph(detail::next_index_t next_index, int const max_parallelism) :
    framework_graph{driver_bundle{std::move(next_index), {}}, max_parallelism}
  {
  }

  framework_graph::framework_graph(driver_bundle bundle, int const max_parallelism) :
    parallelism_limit_{static_cast<std::size_t>(max_parallelism)},
    fixed_hierarchy_{std::move(bundle.hierarchy)},
    driver_{std::move(bundle.driver)},
    src_{graph_,
         [this](tbb::flow_control& fc) mutable -> ready_flushes_then_emit {
           if (auto item = driver_()) {
             return {.ready_flushes = cell_tracker_.report_and_evict_ready_flushes(*item),
                     .index_to_emit = *item};
           }
           fc.stop();
           return {};
         }},
    index_router_{graph_},
    index_receiver_{graph_,
                    tbb::flow::unlimited,
                    [this](ready_flushes_then_emit const& input) -> data_cell_index_ptr {
                      auto&& [ready_flushes, index_to_emit] = input;
                      return index_router_.route(index_to_emit, std::move(ready_flushes));
                    }},
    hierarchy_node_{graph_,
                    tbb::flow::unlimited,
                    [this](data_cell_index_ptr const& index) -> tbb::flow::continue_msg {
                      hierarchy_.increment_count(index);
                      return {};
                    }}
  {
    spdlog::cfg::load_env_levels();
    spdlog::info("Number of worker threads: {}", max_allowed_parallelism::active_value());
  }

  framework_graph::~framework_graph()
  {
    if (shutdown_on_error_) {
      // When in an error state, we need to pop the layer stack and wait for any tasks to finish.
      auto remaining_flushes = cell_tracker_.report_and_evict_ready_flushes(nullptr);
      index_router_.drain(std::move(remaining_flushes));
      graph_.wait_for_all();
    }
  }

  std::size_t framework_graph::seen_cell_count(std::string const& layer_name,
                                               bool const missing_ok) const
  {
    return hierarchy_.count_for(experimental::layer_path(layer_name), missing_ok);
  }

  std::size_t framework_graph::execution_count(std::string const& node_name) const
  {
    return nodes_.execution_count(node_name);
  }

  void framework_graph::execute()
  try {
    finalize();
    run();
  } catch (std::exception const& e) {
    driver_.stop();
    spdlog::error(e.what());
    shutdown_on_error_ = true;
    throw;
  } catch (...) {
    driver_.stop();
    spdlog::error("Unknown exception during graph execution");
    shutdown_on_error_ = true;
    throw;
  }

  void framework_graph::run()
  {
    src_.activate();
    graph_.wait_for_all();

    // Now back out of all remaining layers
    index_router_.drain(cell_tracker_.report_and_evict_ready_flushes(nullptr));
    graph_.wait_for_all();
  }

  void framework_graph::throw_if_registration_errors() const
  {
    if (registration_errors_.empty()) {
      return;
    }
    throw std::runtime_error(
      fmt::format("\nConfiguration errors:\n{}", bulleted_list(registration_errors_)));
  }

  void framework_graph::make_filter_edges()
  {
    // Create filters for predicates and connect them to their consumers
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.predicates));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.observers));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.outputs));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.folds));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.unfolds));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.transforms));
  }

  void framework_graph::make_bookkeeping_edges()
  {
    // Connect the driver node to the index router, which forwards the index to index-set nodes.
    // The hierarchy node is a node that counts how many data cells have been seen for each layer.
    // This information is reported at the end of the job.
    make_edge(src_, index_receiver_);
    make_edge(index_receiver_, hierarchy_node_);
    make_edge(index_router_.unfold_index_receiver(), hierarchy_node_);

    for (auto& node : nodes_.unfolds | std::views::values) {
      make_edge(node->output_index_port(), index_router_.unfold_index_receiver());
      make_edge(node->flush_sender(), index_router_.unfold_flush_receiver());
    }
  }

  void framework_graph::finalize()
  {
    throw_if_registration_errors();
    make_filter_edges();
    make_bookkeeping_edges();

    auto [provider_input_ports, multilayer_join_index_ports] =
      make_computational_edges(nodes_, filters_, graph_);

    if (provider_input_ports.empty()) {
      assert(multilayer_join_index_ports.empty());
      // No algorithms downstream of source.
      return;
    }

    // Index-router finalization makes edges between the index-set nodes and the provider nodes.
    index_router_.finalize(graph_,
                           fixed_hierarchy_.layer_paths(),
                           unfold_layers(nodes_.unfolds),
                           std::move(provider_input_ports),
                           fold_partition_ports(nodes_.folds),
                           std::move(multilayer_join_index_ports));
  }
}
