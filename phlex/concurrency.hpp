#ifndef PHLEX_CONCURRENCY_HPP
#define PHLEX_CONCURRENCY_HPP

#include <cstddef>

namespace phlex::experimental {
  struct concurrency {
    static concurrency const unlimited;
    static concurrency const serial;

    std::size_t value;
  };
}

#endif // PHLEX_CONCURRENCY_HPP
