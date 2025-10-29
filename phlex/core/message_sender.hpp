#ifndef PHLEX_CORE_MESSAGE_SENDER_HPP
#define PHLEX_CORE_MESSAGE_SENDER_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multiplexer.hpp"
#include "phlex/model/fwd.hpp"

#include <map>
#include <stack>

namespace phlex::experimental {

  class message_sender {
  public:
    explicit message_sender(level_hierarchy& hierarchy,
                            multiplexer& mplexer,
                            std::stack<end_of_message_ptr>& eoms);

    void send_flush(product_store_ptr store);
    message make_message(product_store_ptr store);

  private:
    std::size_t original_message_id(product_store_ptr const& store);

    level_hierarchy& hierarchy_;
    multiplexer& multiplexer_;
    std::stack<end_of_message_ptr>& eoms_;
    std::map<level_id_ptr, std::size_t> original_message_ids_;
    std::size_t calls_{};
  };

}

#endif // PHLEX_CORE_MESSAGE_SENDER_HPP
