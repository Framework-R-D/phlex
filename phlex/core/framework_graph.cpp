#include "phlex/core/framework_graph.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/edge_maker.hpp"
#include "phlex/model/data_cell_counter.hpp"
#include "phlex/model/product_store.hpp"

#include "fmt/std.h"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <iostream>

namespace phlex::experimental {
  framework_graph::framework_graph(data_cell_index_ptr index, int const max_parallelism) :
    framework_graph{[index](framework_driver& driver) { driver.yield(index); }, max_parallelism}
  {
  }

  framework_graph::framework_graph(detail::next_index_t next_index, int const max_parallelism) :
    parallelism_limit_{static_cast<std::size_t>(max_parallelism)},
    driver_{std::move(next_index)},
    src_{graph_,
         [this](tbb::flow_control& fc) mutable -> data_cell_index_ptr {
           auto item = driver_();
           if (not item) {
             index_router_.drain();
             fc.stop();
             return {};
           }

           return index_router_.route(*item);
         }},
    index_router_{graph_},
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
      index_router_.drain();
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

    edge_maker make_edges{nodes_.transforms, nodes_.folds, nodes_.unfolds};
    make_edges(graph_,
               index_router_,
               filters_,
               nodes_.outputs,
               nodes_.providers,
               nodes_.predicates,
               nodes_.observers,
               nodes_.folds,
               nodes_.unfolds,
               nodes_.transforms);

    std::map<std::string, flusher_t*> flushers_from_unfolds;
    for (auto const& n : nodes_.unfolds | std::views::values) {
      flushers_from_unfolds.try_emplace(n->child_layer(), &n->flusher());
    }

    // Connect edges between all nodes, the graph-wide flusher, and the unfolds' flushers
    auto connect_with_flusher =
      [this, unfold_flushers = std::move(flushers_from_unfolds)](auto& consumers) {
        for (auto& n : consumers | std::views::values) {
          std::set<flusher_t*> flushers;
          // For providers
          for (product_query const& pq : n->input()) {
            if (auto it = unfold_flushers.find(pq.layer()); it != unfold_flushers.end()) {
              flushers.insert(it->second);
            } else {
              flushers.insert(&index_router_.flusher());
            }
          }
          for (flusher_t* flusher : flushers) {
            make_edge(*flusher, n->flush_port());
          }
        }
      };
    connect_with_flusher(nodes_.folds);

    // The hierarchy node is used to report which data layers have been seen by the
    // framework.  To assemble the report, data-cell indices emitted by the input node are
    // recorded as well as any data-cell indices emitted by an unfold.
    make_edge(src_, hierarchy_node_);
    for (auto& [_, node] : nodes_.unfolds) {
      make_edge(node->output_index_port(), hierarchy_node_);
    }
  }

}
