#ifndef phlex_concurrency_hpp
#define phlex_concurrency_hpp

#include <cstddef>

namespace phlex::experimental {
  struct concurrency {
    static concurrency const unlimited;
    static concurrency const serial;

    std::size_t value;
  };
}

#endif // phlex_concurrency_hpp
