#include "phlex/core/multiplexer.hpp"
#include "phlex/model/product_store.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>

using namespace std::chrono;

namespace {
  phlex::experimental::product_store_const_ptr store_for(
    phlex::experimental::product_store_const_ptr store,
    phlex::experimental::specified_label const& label)
  {
    auto const& [product_name, family, relation] = label;
    if (family.empty()) {
      return store->store_for_product(product_name.full());
    }
    if (store->level_name() == family and store->contains_product(product_name.full())) {
      return store;
    }
    auto parent = store->parent(family);
    if (not parent) {
      return nullptr;
    }
    if (parent->contains_product(product_name.full())) {
      return parent;
    }
    throw std::runtime_error(
      fmt::format("Store not available that provides product {}", label.to_string()));
  }

  struct sender_slot {
    tbb::flow::receiver<phlex::experimental::message>* port;
    phlex::experimental::product_store_const_ptr store;
    void operator()(phlex::experimental::end_of_message_ptr eom, std::size_t message_id) const
    {
      port->try_put({store, eom, message_id});
    }
  };

  std::vector<sender_slot> senders_for(
    phlex::experimental::product_store_const_ptr store,
    phlex::experimental::multiplexer::named_input_ports_t const& ports)
  {
    std::vector<sender_slot> result;
    result.reserve(ports.size());
    for (auto const& [product_label, port] : ports) {
      auto store_to_send = store_for(store, product_label);
      if (not store_to_send) {
        // This is fine if the store is not expected to contain the product.
        continue;
      }

      if (auto const& allowed_family = product_label.family; not allowed_family.empty()) {
        if (store_to_send->level_name() != allowed_family) {
          continue;
        }
      }

      result.push_back({port, store_to_send});
    }
    return result;
  }
}

namespace phlex::experimental {

  multiplexer::multiplexer(tbb::flow::graph& g, bool debug) :
    base{g, tbb::flow::unlimited, std::bind_front(&multiplexer::multiplex, this)}, debug_{debug}
  {
  }

  void multiplexer::finalize(head_ports_t head_ports) { head_ports_ = std::move(head_ports); }

  tbb::flow::continue_msg multiplexer::multiplex(message const& msg)
  {
    ++received_messages_;
    auto const& [store, eom, message_id] = std::tie(msg.store, msg.eom, msg.id);
    if (debug_) {
      spdlog::debug("Multiplexing {} with ID {} (is flush: {})",
                    store->id()->to_string(),
                    message_id,
                    store->is_flush());
    }
    auto start_time = steady_clock::now();

    if (store->is_flush()) {
      for (auto const& head_port : head_ports_ | std::views::values | std::views::join) {
        head_port.port->try_put(msg);
      }
      return {};
    }

    for (auto const& ports : head_ports_ | std::views::values) {
      // FIXME: Should make sure that the received store has a level equal to the most
      //        derived store required by the algorithm.
      auto const senders = senders_for(store, ports);
      if (size(senders) != size(ports)) {
        // Not enough stores to ports of the node
        continue;
      }

      for (auto const& sender : senders) {
        sender(eom, message_id);
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
