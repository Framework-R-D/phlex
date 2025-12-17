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
  product_store_const_ptr store_for(product_store_const_ptr store,
                                    std::string const& port_product_layer)
  {
    if (store->id()->layer_name() == port_product_layer) {
      // This store's layer matches what is expected by the port
      return store;
    }

    if (auto index = store->id()->parent(port_product_layer)) {
      // This store has a parent layer that matches what is expected by the port
      return std::make_shared<product_store>(index, store->source());
    }

    return nullptr;
  }
}

namespace phlex::experimental {

  multiplexer::multiplexer(tbb::flow::graph& g, bool debug) :
    base{g, tbb::flow::unlimited, std::bind_front(&multiplexer::multiplex, this)}, debug_{debug}
  {
  }

  void multiplexer::finalize(input_ports_t provider_input_ports)
  {
    // We must have at least one provider port, or there can be no data to process.
    assert(!provider_input_ports.empty());
    provider_input_ports_ = std::move(provider_input_ports);
  }

  tbb::flow::continue_msg multiplexer::multiplex(message const& msg)
  {
    ++received_messages_;
    auto const& [store, eom, message_id, _] = msg;
    if (debug_) {
      spdlog::debug("Multiplexing {} with ID {} (is flush: {})",
                    store->id()->to_string(),
                    message_id,
                    store->is_flush());
    }

    if (store->is_flush()) {
      for (auto const& [_, port] : provider_input_ports_ | std::views::values) {
        port->try_put(msg);
      }
      return {};
    }

    auto start_time = steady_clock::now();

    for (auto const& [product_label, port] : provider_input_ports_ | std::views::values) {
      if (auto store_to_send = store_for(store, product_label.layer)) {
        port->try_put({std::move(store_to_send), eom, message_id});
      }
    }

    execution_time_ += duration_cast<microseconds>(steady_clock::now() - start_time);
    return {};
  }

  multiplexer::~multiplexer()
  {
    spdlog::debug("Routed {} messages in {} microseconds ({:.3f} microseconds per message)",
                  received_messages_,
                  execution_time_.count(),
                  execution_time_.count() / received_messages_);
  }
}
