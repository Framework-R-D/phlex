#ifndef PHLEX_CORE_MULTIPLEXER_HPP
#define PHLEX_CORE_MULTIPLEXER_HPP

#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex_core_export.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace phlex::experimental {

  class phlex_core_EXPORT multiplexer : public tbb::flow::function_node<message> {
    using base = tbb::flow::function_node<message>;

  public:
    struct named_input_port {
      product_query product_label;
      tbb::flow::receiver<message>* port;
    };
    using named_input_ports_t = std::vector<named_input_port>;
    // map of node name to its input ports
    using head_ports_t = std::map<std::string, named_input_ports_t>;
    using input_ports_t = std::map<std::string, named_input_port>;

    explicit multiplexer(tbb::flow::graph& g, bool debug = false);
    tbb::flow::continue_msg multiplex(message const& msg);

    void finalize(input_ports_t provider_input_ports);

  private:
    input_ports_t provider_input_ports_;
    bool debug_;
    std::atomic<std::size_t> received_messages_{};
  };

}

#endif // PHLEX_CORE_MULTIPLEXER_HPP
