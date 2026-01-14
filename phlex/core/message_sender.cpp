#include "phlex/core/message_sender.hpp"
#include "phlex/core/multiplexer.hpp"
#include "phlex/model/product_store.hpp"

#include <cassert>

namespace phlex::experimental {
  message_sender::message_sender(flusher_t& flusher) : flusher_{flusher} {}

  message message_sender::make_message(product_store_ptr store)
  {
    assert(store);
    assert(not store->is_flush());
    auto const message_id = ++calls_;
    original_message_ids_.try_emplace(store->index(), message_id);
    return {store, message_id, -1ull};
  }

  void message_sender::send_flush(product_store_ptr store)
  {
    assert(store);
    assert(store->is_flush());
    auto const message_id = ++calls_;
    message const msg{store, message_id, original_message_id(store)};
    flusher_.try_put(std::move(msg));
  }

  std::size_t message_sender::original_message_id(product_store_ptr const& store)
  {
    assert(store);
    assert(store->is_flush());

    auto h = original_message_ids_.extract(store->index());
    assert(h);
    return h.mapped();
  }

}
