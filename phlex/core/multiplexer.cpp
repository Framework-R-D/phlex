#include "phlex/core/multiplexer.hpp"
#include "phlex/model/product_store.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stdexcept>

using namespace phlex::experimental;

namespace {
  product_store_const_ptr store_for(product_store_const_ptr store,
                                    std::string_view port_product_layer)
  {
    if (store->index()->layer_name() == port_product_layer) {
      // This store's layer matches what is expected by the port
      return store;
    }

    if (auto index = store->index()->parent(port_product_layer)) {
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
    auto const& [store, message_id, _] = msg;
    if (debug_) {
      spdlog::debug("Multiplexing {} with ID {} (is flush: {})",
                    store->index()->to_string(),
                    message_id,
                    store->is_flush());
    }

    if (store->is_flush()) {
      for (auto const& [_, port] : provider_input_ports_ | std::views::values) {
        port->try_put(msg);
      }
      return {};
    }

    for (auto const& [product_label, port] : provider_input_ports_ | std::views::values) {
      if (auto store_to_send =
            store_for(store, std::string_view(identifier(product_label.layer)))) {
        port->try_put({std::move(store_to_send), message_id});
      }
    }

    return {};
  }
}
