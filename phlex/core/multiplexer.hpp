#ifndef PHLEX_CORE_MULTIPLEXER_HPP
#define PHLEX_CORE_MULTIPLEXER_HPP

#include "phlex/core/message.hpp"
#include "phlex/model/level_id.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace phlex::experimental {

  class multiplexer : public tbb::flow::function_node<message> {
    using base = tbb::flow::function_node<message>;

  public:
    ~multiplexer();

    struct named_input_port {
      product_query product_label;
      tbb::flow::receiver<message>* port;
    };
    using named_input_ports_t = std::vector<named_input_port>;
    using head_ports_t = std::map<std::string, named_input_ports_t>;

    explicit multiplexer(tbb::flow::graph& g, bool debug = false);
    tbb::flow::continue_msg multiplex(message const& msg);

    void finalize(head_ports_t head_ports);

    head_ports_t const& downstream_ports() const noexcept { return head_ports_; }

  private:
    head_ports_t head_ports_;
    bool debug_;
    std::atomic<std::size_t> received_messages_{};
    std::chrono::duration<float, std::chrono::microseconds::period> execution_time_{};
  };

}

#endif // PHLEX_CORE_MULTIPLEXER_HPP
