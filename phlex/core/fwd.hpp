#ifndef PHLEX_CORE_FWD_HPP
#define PHLEX_CORE_FWD_HPP

#include "phlex/model/fwd.hpp"

#include "oneapi/tbb/flow_graph.h"

namespace phlex::experimental {
  class consumer;
  class declared_output;
  class generator;
  struct flush_message;
  class framework_graph;
  struct message;
  class index_router;
  class products_consumer;

  using flusher_t = tbb::flow::broadcast_node<flush_message>;
}

#endif // PHLEX_CORE_FWD_HPP
