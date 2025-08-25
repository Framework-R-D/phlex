#ifndef phlex_concurrency_hpp
#define phlex_concurrency_hpp

#include "oneapi/tbb/global_control.h"

namespace phlex::experimental {
  struct concurrency {
    static concurrency const unlimited;
    static concurrency const serial;

    std::size_t value;

    class max_allowed_parallelism {
    public:
      explicit max_allowed_parallelism(std::size_t const n) :
        control_{tbb::global_control::max_allowed_parallelism, n}
      {
      }

      static auto active_value()
      {
        using control = tbb::global_control;
        return control::active_value(control::max_allowed_parallelism);
      }

    private:
      tbb::global_control control_;
    };
  };
}

#endif // phlex_concurrency_hpp
