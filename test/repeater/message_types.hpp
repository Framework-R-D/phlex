#ifndef TEST_MESSAGE_TYPES_HPP
#define TEST_MESSAGE_TYPES_HPP

#include "oneapi/tbb/flow_graph.h"
#include "phlex/model/data_cell_index.hpp"
#include "spdlog/spdlog.h"

#include <cstddef>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace phlex::test {
  struct index_message {
    std::size_t msg_id;
    data_cell_index_ptr index;
    bool cache{true};
  };

  template <typename T>
  struct indexed_message {
    std::size_t msg_id;
    data_cell_index_ptr index;
    T data;
  };

  struct indexed_end_token {
    data_cell_index_ptr index;
    int count;
  };

  struct named_index_port {
    std::string layer;
    tbb::flow::receiver<indexed_end_token>* token_port;
    tbb::flow::receiver<index_message>* index_port;
  };
  using named_index_ports = std::vector<named_index_port>;

  template <typename... Ts>
  using indexed_message_tuple = std::tuple<indexed_message<Ts>...>;

  struct no_more_indices {};

  using message_from_input = std::variant<data_cell_index_ptr, no_more_indices>;

  struct index_message_matcher {
    std::size_t operator()(index_message const& msg) const noexcept { return msg.msg_id; }
  };

  template <typename T>
  struct indexed_message_matcher {
    std::size_t operator()(indexed_message<T> const& msg) const noexcept { return msg.msg_id; }
  };
}

#endif // TEST_MESSAGE_TYPES_HPP
