#include "phlex/core/concepts.hpp"

using namespace phlex::experimental;

namespace {
  // FIXME: Should is_transform_like support a function signature that
  //        takes an argument by non-const reference?  It can make
  //        sense if the function receives an argument corresponding
  //        to a (mutable) resource.  But not for a data product...
  int transform [[maybe_unused]] (double&) { return 1; };
  void not_a_transform [[maybe_unused]] (int) {}

  struct struct_with_member_function {
    int call(int, int) const noexcept { return 1; };
  };
}

int main()
{
  static_assert(is_transform_like<decltype(transform)>);
  static_assert(is_transform_like<decltype(&struct_with_member_function::call)>);
  static_assert(not is_transform_like<decltype(not_a_transform)>);

  static_assert(not is_observer_like<decltype(transform)>);
  static_assert(is_observer_like<decltype(not_a_transform)>);
}
