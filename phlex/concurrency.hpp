#ifndef PHLEX_CONCURRENCY_HPP
#define PHLEX_CONCURRENCY_HPP

#include "phlex/phlex_core_export.hpp"

#include <cstddef>

namespace phlex {
  struct PHLEX_CORE_EXPORT concurrency {
    static PHLEX_CORE_EXPORT concurrency const unlimited;
    static PHLEX_CORE_EXPORT concurrency const serial;

    std::size_t value;
  };
}

#endif // PHLEX_CONCURRENCY_HPP
