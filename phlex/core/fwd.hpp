#ifndef PHLEX_CORE_FWD_HPP
#define PHLEX_CORE_FWD_HPP

#include "phlex/model/fwd.hpp"

#include "oneapi/tbb/flow_graph.h"

namespace phlex::experimental {
  class component;
  class consumer;
  class declared_output;
  class generator;
  class framework_graph;
  struct message;
  class message_sender;
  class multiplexer;
  class products_consumer;

  using flusher_t = tbb::flow::broadcast_node<message>;
}

#endif // PHLEX_CORE_FWD_HPP

// Local Variables:
// mode: c++
// End:
