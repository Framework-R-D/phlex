#ifndef PHLEX_CORE_FWD_HPP
#define PHLEX_CORE_FWD_HPP

#include "phlex/model/fwd.hpp"
#include "phlex/phlex_core_export.hpp"

namespace phlex::detail {
  class consumer;
  class PHLEX_CORE_EXPORT declared_fold;
  class PHLEX_CORE_EXPORT declared_observer;
  class PHLEX_CORE_EXPORT declared_output;
  class PHLEX_CORE_EXPORT declared_predicate;
  class PHLEX_CORE_EXPORT declared_transform;
  class PHLEX_CORE_EXPORT declared_unfold;
  class PHLEX_CORE_EXPORT provider_node;
  class generator;
  class framework_graph;
  struct message;
  class index_router;
}

#endif // PHLEX_CORE_FWD_HPP
