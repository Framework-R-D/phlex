#ifndef PHLEX_CONCURRENCY_HPP
#define PHLEX_CONCURRENCY_HPP

#include "phlex_core_export.hpp"

#include <cstddef>

namespace phlex {
  struct phlex_core_EXPORT concurrency {
    static concurrency const unlimited;
    static concurrency const serial;

    std::size_t value;
  };
}

#endif // PHLEX_CONCURRENCY_HPP
