#ifndef PHLEX_CORE_MULTIPLEXER_HPP
#define PHLEX_CORE_MULTIPLEXER_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_counter.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <functional>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>

namespace phlex::experimental {
  class layer_sentry {
  public:
    layer_sentry(flush_counters& counters,
                 flusher_t& flusher,
                 data_cell_index_ptr index,
                 std::size_t message_id);
    ~layer_sentry();
    std::size_t depth() const;

  private:
    flush_counters& counters_;
    flusher_t& flusher_;
    data_cell_index_ptr index_;
    std::size_t message_id_;
  };

  class multiplexer {
  public:
    using index_set_node = tbb::flow::broadcast_node<index_message>;

    struct named_input_port {
      product_query product_label;
      tbb::flow::receiver<message>* port;
    };
    using named_input_ports_t = std::vector<named_input_port>;

    // map of node name to its input ports
    using head_ports_t = std::map<std::string, named_input_ports_t>;

    struct provider_input_port_t {
      product_query product_label;
      tbb::flow::receiver<index_message>* port;
    };
    using provider_input_ports_t = std::map<std::string, provider_input_port_t>;

    explicit multiplexer(tbb::flow::graph& g);
    data_cell_index_ptr route(data_cell_index_ptr index);

    void finalize(tbb::flow::graph& g, provider_input_ports_t provider_input_ports);
    void drain();
    flusher_t& flusher() { return flusher_; }

  private:
    void backout_to(data_cell_index_ptr store);

    provider_input_ports_t provider_input_ports_;
    std::atomic<std::size_t> received_indices_{};
    flusher_t flusher_;
    flush_counters counters_;
    std::stack<layer_sentry> layers_;

    using broadcasters_t = std::unordered_map<std::string, index_set_node>;
    broadcasters_t broadcasters_;
  };

}

#endif // PHLEX_CORE_MULTIPLEXER_HPP
