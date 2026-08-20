#include "phlex/core/concepts.hpp"
#include "phlex/core/framework_graph.hpp"

using namespace phlex::detail;

namespace {
  int transform [[maybe_unused]] (double&) { return 1; };
  void not_a_transform [[maybe_unused]] (int) {}

  struct A {
    int call(int, int) const noexcept { return 1; };
  };
}

int main()
{
  static_assert(is_transform_like<decltype(transform)>);
  static_assert(is_transform_like<decltype(&A::call)>);
  static_assert(not is_transform_like<decltype(not_a_transform)>);

  static_assert(not is_observer_like<decltype(transform)>);
  static_assert(is_observer_like<decltype(not_a_transform)>);
}
