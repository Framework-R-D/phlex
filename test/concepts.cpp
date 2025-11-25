#include "phlex/core/concepts.hpp"

using namespace phlex::experimental;

namespace {
  auto transform [[maybe_unused]] (double&) -> int { return 1; };
  void not_a_transform [[maybe_unused]] (int) {}

  struct A {
    [[nodiscard]] auto call(int, int) const noexcept -> int { return 1; };
  };
}

auto main() -> int
{
  static_assert(is_transform_like<decltype(transform)>);
  static_assert(is_transform_like<decltype(&A::call)>);
  static_assert(not is_transform_like<decltype(not_a_transform)>);

  static_assert(not is_observer_like<decltype(transform)>);
  static_assert(is_observer_like<decltype(not_a_transform)>);
}
