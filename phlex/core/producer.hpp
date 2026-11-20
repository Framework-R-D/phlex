#ifndef PHLEX_CORE_PRODUCER_HPP
#define PHLEX_CORE_PRODUCER_HPP

#include "phlex/core/message.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/phlex_core_export.hpp"

#include <oneapi/tbb/flow_graph.h>

namespace phlex::detail {
  /// Independent producer interface used by the catalog, including translators.
  /// Outputs carry explicit empty or named `suffix_t` values; `name()` retains the creator's
  /// plugin component. The producer must outlive edges using its borrowed output port.
  class PHLEX_CORE_EXPORT producer {
  public:
    virtual ~producer() = default;

    virtual phlex::experimental::algorithm_name const& name() const noexcept = 0;
    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual phlex::experimental::product_specifications const& output() const = 0;
  };
}

#endif // PHLEX_CORE_PRODUCER_HPP
