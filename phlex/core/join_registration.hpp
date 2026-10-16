#ifndef PHLEX_CORE_JOIN_REGISTRATION_HPP
#define PHLEX_CORE_JOIN_REGISTRATION_HPP

#include "phlex/core/message.hpp"
#include "phlex/model/identifier.hpp"

#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <vector>

namespace phlex::detail {
  // Required non-owning receivers for one product repeater or fold accumulator.
  // The layer identifies storage and invocation-index projection, not an independent count.
  struct join_participant {
    phlex::experimental::identifier layer;
    gsl::not_null<tbb::flow::receiver<index_message>*> index_port;
    gsl::not_null<tbb::flow::receiver<indexed_end_token>*> completion_port;
  };

  // Product inputs drive invocations; all participants constrain their ancestry.
  // Empty participants mean that no router registration is needed.
  struct join_registration {
    std::vector<phlex::experimental::identifier> input_layers;
    std::vector<join_participant> participants;
  };
}

#endif // PHLEX_CORE_JOIN_REGISTRATION_HPP
