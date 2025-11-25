#include "phlex/core/framework_graph.hpp"

#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/edge_maker.hpp"
#include "phlex/core/filter.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/message_sender.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/level_counter.hpp"

#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <oneapi/tbb/flow_graph.h>
#include <stdexcept>
#include <utility>

namespace phlex::experimental {
  level_sentry::level_sentry(flush_counters& counters,
                             message_sender& sender,
                             product_store_ptr store) :
    counters_{counters}, sender_{sender}, store_{std::move(store)}, depth_{store_->id()->depth()}
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

  auto level_sentry::depth() const noexcept -> std::size_t { return depth_; }

  framework_graph::framework_graph(product_store_ptr store, int const max_parallelism) :
    framework_graph{[store](framework_driver& driver) -> void { driver.yield(store); }, max_parallelism}
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
    spdlog::info("Number of worker threads: {}", max_allowed_parallelism::active_value());

    // The parent of the job message is null
    eoms_.push(nullptr);
  }

  framework_graph::~framework_graph() = default;

  auto framework_graph::execution_counts(std::string const& node_name) const -> std::size_t
  {
    return nodes_.execution_counts(node_name);
  }

  auto framework_graph::product_counts(std::string const& node_name) const -> std::size_t
  {
    return nodes_.product_counts(node_name);
  }

  void framework_graph::execute()
  {
    finalize();
    run();
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
    make_edges(src_,
               multiplexer_,
               filters_,
               nodes_.outputs,
               nodes_.predicates,
               nodes_.observers,
               nodes_.folds,
               nodes_.unfolds,
               nodes_.transforms);
  }

  auto framework_graph::accept(product_store_ptr store) -> product_store_ptr
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
