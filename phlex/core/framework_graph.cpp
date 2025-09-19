#include "phlex/core/framework_graph.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/edge_maker.hpp"
#include "phlex/model/level_counter.hpp"
#include "phlex/model/product_store.hpp"

#include "spdlog/cfg/env.h"

#include <cassert>
#include <iostream>

namespace phlex::experimental {
  level_sentry::level_sentry(flush_counters& counters,
                             message_sender& sender,
                             product_store_ptr store) :
    counters_{counters}, sender_{sender}, store_{store}, depth_{store_->id()->depth()}
  {
    counters_.update(store_->id());
  }

  level_sentry::~level_sentry()
  {
    auto flush_result = counters_.extract(store_->id());
    auto flush_store = store_->make_flush();
    if (not flush_result.empty()) {
      flush_store->add_product("[flush]",
                               std::make_shared<flush_counts const>(std::move(flush_result)));
    }
    sender_.send_flush(std::move(flush_store));
  }

  std::size_t level_sentry::depth() const noexcept { return depth_; }

  framework_graph::framework_graph(product_store_ptr store, int const max_parallelism) :
    framework_graph{[store](framework_driver& driver) { driver.yield(store); }, max_parallelism}
  {
  }

  // FIXME: The algorithm below should support user-specified flush stores.
  framework_graph::framework_graph(detail::next_store_t next_store, int const max_parallelism) :
    parallelism_limit_{static_cast<std::size_t>(max_parallelism)},
    driver_{std::move(next_store)},
    src_{graph_,
         [this](tbb::flow_control& fc) mutable -> message {
           auto item = driver_();
           if (not item) {
             drain();
             fc.stop();
             return {};
           }
           auto store = *item;
           assert(not store->is_flush());
           return sender_.make_message(accept(std::move(store)));
         }},
    multiplexer_{graph_}
  {
    // FIXME: This requirement is in place so that the yielding driver can be used.
    //        At least 2 threads are required for that to work.
    //        It would be better if the specified concurrency would be applied to an
    //        arena in which the user-facing work is done.
    if (max_parallelism < 2) {
      throw std::runtime_error("Must choose concurrency level of at least 2.");
    }

    // FIXME: Should the loading of env levels happen in the phlex app only?
    spdlog::cfg::load_env_levels();
    spdlog::info("Number of worker threads: {}",
                 concurrency::max_allowed_parallelism::active_value());

    // The parent of the job message is null
    eoms_.push(nullptr);
  }

  framework_graph::~framework_graph() = default;

  std::size_t framework_graph::execution_counts(std::string const& node_name) const
  {
    return nodes_.execution_counts(node_name);
  }

  std::size_t framework_graph::product_counts(std::string const& node_name) const
  {
    return nodes_.product_counts(node_name);
  }

  void framework_graph::execute(std::string const& dot_file_prefix)
  {
    finalize(dot_file_prefix);
    run();
    // post_data_graph(dot_file_prefix);
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

  void framework_graph::finalize(std::string const& dot_file_prefix)
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

    edge_maker make_edges{dot_file_prefix, nodes_.transforms, nodes_.folds};
    make_edges(src_,
               multiplexer_,
               filters_,
               nodes_.outputs,
               consumers{nodes_.predicates, {.shape = "box"}},
               consumers{nodes_.observers, {.shape = "box"}},
               consumers{nodes_.folds, {.shape = "invtrapezium"}},
               consumers{nodes_.unfolds, {.shape = "trapezium"}},
               consumers{nodes_.transforms, {.shape = "box"}});

    if (auto data_graph = make_edges.release_data_graph()) {
      data_graph->to_file(dot_file_prefix);
    }
    if (auto function_graph = make_edges.release_function_graph()) {
      function_graph->to_file(dot_file_prefix);
    }
  }

  product_store_ptr framework_graph::accept(product_store_ptr store)
  {
    assert(store);
    auto const new_depth = store->id()->depth();
    while (not empty(levels_) and new_depth <= levels_.top().depth()) {
      levels_.pop();
      eoms_.pop();
    }
    levels_.emplace(counters_, sender_, store);
    return store;
  }

  void framework_graph::drain()
  {
    while (not empty(levels_)) {
      levels_.pop();
      eoms_.pop();
    }
  }
}
