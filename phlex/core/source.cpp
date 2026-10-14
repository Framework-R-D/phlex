#include "phlex/core/source.hpp"

#include "phlex/model/index_generator.hpp"

namespace phlex::detail {
  // Clang-tidy misdiagnoses the coroutine's generated promise_type access.
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  index_generator source::indices() { co_return; }
}
