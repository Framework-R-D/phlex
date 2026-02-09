#include "phlex/core/multiplexer.hpp"
#include "phlex/model/product_store.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stdexcept>

using namespace std::chrono;
using namespace phlex::experimental;

namespace {
  phlex::data_cell_index_ptr index_for(phlex::data_cell_index_ptr const index,
                                       std::string const& port_product_layer)
  {
    if (index->layer_name() == port_product_layer) {
      // This index's layer matches what is expected by the port
      return index;
    }

    if (auto parent_index = index->parent(port_product_layer)) {
      // This index has a parent layer that matches what is expected by the port
      return parent_index;
    }

    return nullptr;
  }
}

namespace phlex::experimental {
  layer_sentry::layer_sentry(flush_counters& counters,
                             flusher_t& flusher,
                             data_cell_index_ptr index,
                             std::size_t const message_id) :
    counters_{counters}, flusher_{flusher}, index_{index}, message_id_{message_id}
  {
    counters_.update(index_);
  }

  layer_sentry::~layer_sentry()
  {
    // To consider: We may want to skip the following logic if the framework prematurely
    //              needs to shut down.  Keeping it enabled allows in-flight folds to
    //              complete.  However, in some cases it may not be desirable to do this.
    auto flush_result = counters_.extract(index_);
    flush_counts_ptr result;
    if (not flush_result.empty()) {
      result = std::make_shared<flush_counts const>(std::move(flush_result));
    }
    flusher_.try_put({index_, std::move(result), message_id_});
  }

  std::size_t layer_sentry::depth() const { return index_->depth(); }

  multiplexer::multiplexer(tbb::flow::graph& g) : flusher_{g} {}

  void multiplexer::finalize(provider_input_ports_t provider_input_ports)
  {
    // We must have at least one provider port, or there can be no data to process.
    assert(!provider_input_ports.empty());
    provider_input_ports_ = std::move(provider_input_ports);
  }

  data_cell_index_ptr multiplexer::route(data_cell_index_ptr const index)
  {
    backout_to(index);

    auto message_id = received_indices_.fetch_add(1);
    layers_.emplace(counters_, flusher_, index, message_id);

    auto start_time = steady_clock::now();

    for (auto const& [product_label, port] : provider_input_ports_ | std::views::values) {
      if (auto index_to_send = index_for(index, product_label.layer())) {
        port->try_put({std::move(index_to_send), message_id});
      }
    }

    execution_time_ += duration_cast<microseconds>(steady_clock::now() - start_time);
    return index;
  }

  multiplexer::~multiplexer()
  {
    spdlog::debug("Routed {} messages in {} microseconds ({:.3f} microseconds per message)",
                  received_indices_,
                  execution_time_.count(),
                  execution_time_.count() / received_indices_);
  }

  void multiplexer::backout_to(data_cell_index_ptr const index)
  {
    assert(index);
    auto const new_depth = index->depth();
    while (not empty(layers_) and new_depth <= layers_.top().depth()) {
      layers_.pop();
    }
  }

  void multiplexer::drain()
  {
    while (not empty(layers_)) {
      layers_.pop();
    }
  }
}
