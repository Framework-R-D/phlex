#include "phlex/core/message_sender.hpp"
#include "phlex/core/end_of_message.hpp"
#include "phlex/core/multiplexer.hpp"
#include "phlex/model/product_store.hpp"

#include <cassert>

namespace phlex::experimental {
  message_sender::message_sender(level_hierarchy& hierarchy,
                                 multiplexer& mplexer,
                                 std::stack<end_of_message_ptr>& eoms) :
    hierarchy_{hierarchy}, multiplexer_{mplexer}, eoms_{eoms}
  {
  }

  message message_sender::make_message(product_store_ptr store)
  {
    assert(store);
    assert(not store->is_flush());
    auto const message_id = ++calls_;
    original_message_ids_.try_emplace(store->id(), message_id);
    auto parent_eom = eoms_.top();
    end_of_message_ptr current_eom{};
    if (parent_eom == nullptr) {
      current_eom = eoms_.emplace(end_of_message::make_base(&hierarchy_, store->id()));
    }
    else {
      current_eom = eoms_.emplace(parent_eom->make_child(store->id()));
    }
    return {store, current_eom, message_id, -1ull};
  }

  void message_sender::send_flush(product_store_ptr store)
  {
    assert(store);
    assert(store->is_flush());
    auto const message_id = ++calls_;
    message const msg{store, nullptr, message_id, original_message_id(store)};
    multiplexer_.try_put(std::move(msg));
  }

  std::size_t message_sender::original_message_id(product_store_ptr const& store)
  {
    assert(store);
    assert(store->is_flush());

    auto h = original_message_ids_.extract(store->id());
    assert(h);
    return h.mapped();
  }

}
