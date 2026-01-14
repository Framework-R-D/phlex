#ifndef PHLEX_CORE_MESSAGE_SENDER_HPP
#define PHLEX_CORE_MESSAGE_SENDER_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/fwd.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <map>

namespace phlex::experimental {

  using flusher_t = tbb::flow::broadcast_node<message>;

  class message_sender {
  public:
    explicit message_sender(flusher_t& flusher);

    void send_flush(product_store_ptr store);
    message make_message(product_store_ptr store);

  private:
    std::size_t original_message_id(product_store_ptr const& store);

    flusher_t& flusher_;
    std::map<data_cell_index_ptr, std::size_t> original_message_ids_;
    std::size_t calls_{};
  };

}

#endif // PHLEX_CORE_MESSAGE_SENDER_HPP
