#include "phlex/core/framework_graph.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/edge_maker.hpp"
#include "phlex/model/product_store.hpp"

#include "fmt/std.h"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <iostream>

namespace phlex::experimental {
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
                      return index_router_.route(index_to_emit, ready_flushes);
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
      // When in an error state, we need to sanely pop the layer stack and wait for any tasks to finish.
      auto remaining_flushes = cell_tracker_.report_and_evict_ready_flushes(nullptr);
      index_router_.drain(std::move(remaining_flushes));
      graph_.wait_for_all();
    }
  }

  std::size_t framework_graph::seen_cell_count(std::string const& layer_name,
                                               bool const missing_ok) const
  {
    return hierarchy_.count_for(layer_name, missing_ok);
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
          throw std::runtime_error("A non-existent filter with the name '" + predicate_name +
                                   "' was specified for " + name);
        }
      }
      return result;
    }
  }

  void framework_graph::finalize()
  {
    if (not empty(registration_errors_)) {
      std::string error_msg{"\nConfiguration errors:\n"};
      for (auto const& error : registration_errors_) {
        error_msg += "  - " + error + '\n';
      }
      throw std::runtime_error(error_msg);
    }

    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.predicates));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.observers));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.outputs));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.folds));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.unfolds));
    filters_.merge(internal_edges_for_predicates(graph_, nodes_.predicates, nodes_.transforms));

    std::set<identifier> unfold_input_layer_names;
    // Count how many distinct unfold nodes consume each input layer.  When that count is
    // greater than one, the flush_gate for an index in that layer must collect a flush
    // message from every unfold before it knows the total number of children it will see.
    std::map<identifier, std::size_t> unfold_count_per_input_layer;
    for (auto const& n : nodes_.unfolds | std::views::values) {
      for (auto const& input : n->input()) {
        if (!static_cast<identifier const&>(input.layer).empty()) {
          unfold_input_layer_names.insert(input.layer);
          ++unfold_count_per_input_layer[identifier{input.layer}];
        }
      }
    }

    std::vector<identifier> unfold_output_layer_names;
    for (auto const& n : nodes_.unfolds | std::views::values) {
      unfold_output_layer_names.emplace_back(n->child_layer());
    }

    index_router_.establish_layers(
      fixed_hierarchy_.layer_paths(),
      std::vector<identifier>(unfold_input_layer_names.begin(), unfold_input_layer_names.end()),
      unfold_output_layer_names);
    index_router_.register_unfold_count_per_input_layer(std::move(unfold_count_per_input_layer));

    edge_maker make_edges{nodes_.transforms, nodes_.folds, nodes_.unfolds};
    auto [provider_input_ports, multilayer_join_index_ports] =
      make_edges(filters_,
                 nodes_.outputs,
                 nodes_.providers,
                 // Consumers of data products below
                 nodes_.predicates,
                 nodes_.observers,
                 nodes_.folds,
                 nodes_.unfolds,
                 nodes_.transforms);
    if (not std::empty(provider_input_ports)) {
      index_router_.finalize(
        graph_, std::move(provider_input_ports), std::move(multilayer_join_index_ports));
    }

    // The hierarchy node is used to report which data layers have been seen by the
    // framework.  To assemble the report, data-cell indices emitted by the input node are
    // recorded as well as any data-cell indices emitted by an unfold.

    // FIXME: Eventually the separate index_receiver_ and index_router_.unfold_index_receiver()
    //        may be combined.
    make_edge(src_, index_receiver_);
    make_edge(index_receiver_, hierarchy_node_);
    make_edge(index_router_.unfold_index_receiver(), hierarchy_node_);

    for (auto& [_, node] : nodes_.folds) {
      make_edge(index_router_.flusher(), node->flush_port());
    }

    for (auto& [_, node] : nodes_.unfolds) {
      make_edge(node->output_index_port(), index_router_.unfold_index_receiver());
      make_edge(node->flush_sender(), index_router_.unfold_flush_receiver());
    }
  }
}
