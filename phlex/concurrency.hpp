#ifndef PHLEX_CONCURRENCY_HPP
#define PHLEX_CONCURRENCY_HPP

#include "phlex/phlex_core_export.hpp"

#include <cstddef>

namespace phlex {
  /// @brief Maximum number of simultaneous calls of one registered algorithm.
  ///
  /// Passed when registering an algorithm (transform, fold, unfold, observe, predicate,
  /// provide or output); every registration function defaults to @c serial.
  ///
  /// The limit applies to a single node only.  It does not order the calls (data cells
  /// may arrive in any order, even for a serial node), does not tie the calls to one
  /// thread, and does not exclude other nodes, even nodes bound to the same object.
  struct PHLEX_CORE_EXPORT concurrency {
    /// No limit beyond the number of threads available to the job.
    static PHLEX_CORE_EXPORT concurrency const unlimited;
    /// At most one call at a time.
    static PHLEX_CORE_EXPORT concurrency const serial;

    /// Maximum number of simultaneous calls; use @c concurrency{n} for a limit of @c n.
    /// A value of 0 means unlimited.
    std::size_t value;
  };
}

#endif // PHLEX_CONCURRENCY_HPP
