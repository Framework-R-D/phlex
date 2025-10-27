#include "phlex/metaprogramming/type_deduction.hpp"
#include <tuple>
#include <type_traits>

using namespace phlex::experimental;

namespace {
  auto transform [[maybe_unused]] (double&) -> int { return 1; };
  void observe [[maybe_unused]] (int) {}
  void only_void_param [[maybe_unused]] () {}
  auto still_no_output [[maybe_unused]] () -> std::tuple<> { return {}; }
  auto two_output_objects [[maybe_unused]] (int, double) -> std::tuple<int, double> { return {}; }
  auto closure [[maybe_unused]] = [](int) -> double { return 2.; };
}

auto main() -> int
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
