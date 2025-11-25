#include "phlex/metaprogramming/type_deduction.hpp"

#include <tuple>
#include <type_traits>

using namespace phlex::experimental;

namespace {
  int transform [[maybe_unused]] (double&) { return 1; };
  void observe [[maybe_unused]] (int) {}
  // The next function simulates a user function that specifies 'void' as an input parameter.
  void only_void_param [[maybe_unused]] (void) {} // NOLINT(modernize-redundant-void-arg)
  std::tuple<> still_no_output [[maybe_unused]] () { return {}; }
  std::tuple<int, double> two_output_objects [[maybe_unused]] (int, double) { return {}; }
  auto const closure [[maybe_unused]] = [](int) -> double { return 2.; };
}

int main()
{
  static_assert(std::is_same<return_type<decltype(transform)>, int>{});
  static_assert(std::is_same<return_type<decltype(observe)>, void>{});
  static_assert(std::is_same<return_type<decltype(only_void_param)>, void>{});
  static_assert(std::is_same<return_type<decltype(still_no_output)>, std::tuple<>>{});
  static_assert(std::is_same<return_type<decltype(two_output_objects)>, std::tuple<int, double>>{});
  static_assert(std::is_same<return_type<decltype(closure)>, double>{});

  static_assert(number_parameters<decltype(transform)> == 1);
  static_assert(number_parameters<decltype(observe)> == 1);
  static_assert(number_parameters<decltype(only_void_param)> == 0);
  static_assert(number_parameters<decltype(still_no_output)> == 0);
  static_assert(number_parameters<decltype(two_output_objects)> == 2);

  static_assert(number_output_objects<decltype(transform)> == 1);
  static_assert(number_output_objects<decltype(observe)> == 0);
  static_assert(number_output_objects<decltype(only_void_param)> == 0);
  static_assert(number_output_objects<decltype(still_no_output)> == 0);
  static_assert(number_output_objects<decltype(two_output_objects)> == 2);
}
