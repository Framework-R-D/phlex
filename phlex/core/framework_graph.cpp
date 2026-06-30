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
  framework_graph framework_graph::with_default_driver(int const max_parallelism)
  {
    return framework_graph{driver_mode::default_driver, max_parallelism};
  }

  framework_graph framework_graph::without_driver(int const max_parallelism)
  {
    return framework_graph{driver_mode::deferred_driver, max_parallelism};
  }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  framework_graph::framework_graph(driver_mode const mode, int const max_parallelism) :
    parallelism_limit_{static_cast<std::size_t>(max_parallelism)},
    src_{graph_,
         [this](tbb::flow_control& fc) mutable -> phlex::detail::ready_flushes_then_emit {
           assert(driver_);
           if (auto item = (*driver_)()) {
             return {.ready_flushes = cell_tracker_.report_and_evict_ready_flushes(*item),
                     .index_to_emit = *item};
           }
           fc.stop();
           return {};
         }},
    index_router_{graph_},
    index_receiver_{
      graph_,
      tbb::flow::unlimited,
      [this](phlex::detail::ready_flushes_then_emit const& input) -> phlex::data_cell_index_ptr {
        auto&& [ready_flushes, index_to_emit] = input;
        return index_router_.route(index_to_emit, std::move(ready_flushes));
      }},
    hierarchy_node_{graph_,
                    tbb::flow::unlimited,
                    [this](phlex::data_cell_index_ptr const& index) -> tbb::flow::continue_msg {
                      hierarchy_.increment_count(index);
                      return {};
                    }},
    driver_mode_{mode}
  {
    if (driver_mode_ == driver_mode::default_driver) {
      driver_.emplace([](phlex::detail::framework_driver& driver) {
        driver.yield(phlex::data_cell_index::job());
      });
    }

    spdlog::cfg::load_env_levels();
    spdlog::info("Number of worker threads: {}",
                 phlex::detail::max_allowed_parallelism::active_value());
  }

  void framework_graph::add_driver(driver_bundle bundle)
  {
    if (driver_mode_ != driver_mode::deferred_driver) {
      throw std::runtime_error(
        "Cannot configure framework_graph with a driver when not in deferred mode.");
    }
    if (driver_) {
      throw std::runtime_error("Driver has already been configured for framework_graph.");
    }
    if (!bundle.driver) {
      throw std::runtime_error("Cannot configure framework_graph with an empty driver.");
    }
    fixed_hierarchy_ = std::move(bundle.hierarchy);
    driver_.emplace(std::move(bundle.driver));
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
    return hierarchy_.count_for(experimental::layer_path(layer_name), missing_ok);
  }

  std::size_t framework_graph::execution_count(std::string const& node_name) const
  {
    return nodes_.execution_count(node_name);
  }

  void framework_graph::execute()
  {
    if (!driver_) {
      throw std::runtime_error("No driver configured for framework_graph.");
    }

    try {
      finalize();
      run();
    } catch (std::exception const& e) {
      driver_->stop();

      spdlog::error(e.what());
      shutdown_on_error_ = true;
      throw;
    } catch (...) {
      driver_->stop();
      spdlog::error("Unknown exception during graph execution");
      shutdown_on_error_ = true;
      throw;
    }
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
          throw std::runtime_error(std::format(
            "A non-existent filter with the name '{}' was specified for {}", predicate_name, name));
        }
      }
      return result;
    }
  }

  void framework_graph::throw_if_registration_errors() const
  {
    if (registration_errors_.empty()) {
      return;
    }
    throw std::runtime_error(fmt::format("\nConfiguration errors:\n{}",
                                         phlex::detail::bulleted_list(registration_errors_)));
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

    for (auto& node : nodes_.folds | std::views::values) {
      make_edge(index_router_.flusher(), node->flush_port());
    }

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
    finalize_router(std::move(provider_input_ports), std::move(multilayer_join_index_ports));
  }

  // FIXME: Much, if not all, of this logic should be moved to the index_router.
  void framework_graph::finalize_router(
    index_router::provider_input_ports_t provider_input_ports,
    std::map<std::string, named_index_ports> multilayer_join_index_ports)
  {
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

    // FIXME: All of this should be collapsed into one call to index_router::finalize()
    index_router_.establish_layers(
      fixed_hierarchy_.layer_paths(),
      std::vector<identifier>(unfold_input_layer_names.begin(), unfold_input_layer_names.end()),
      unfold_output_layer_names);
    index_router_.register_unfold_count_per_input_layer(std::move(unfold_count_per_input_layer));
    index_router_.finalize(
      graph_, std::move(provider_input_ports), std::move(multilayer_join_index_ports));
  }
}
