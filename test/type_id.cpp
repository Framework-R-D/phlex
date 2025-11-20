#include "phlex/model/type_id.hpp"

#include "fmt/format.h"

#include <array>
#include <atomic>
#include <cassert>
#include <functional>
#include <vector>

using namespace phlex::experimental;

struct A {
  int a;
  int b;
  char c;

  std::atomic<int> d;
};

int main()
{
  static_assert(make_type_id<bool>().fundamental() == type_id::builtin::bool_v);
  static_assert(make_type_id<char>().fundamental() == type_id::builtin::char_v);
  static_assert(make_type_id<int>().fundamental() == type_id::builtin::int_v);
  static_assert(make_type_id<double>().fundamental() == type_id::builtin::double_v);
  static_assert(make_type_id<long>().fundamental() == type_id::builtin::long_v);
  static_assert(make_type_id<long long>().fundamental() == type_id::builtin::long_long_v);

  static_assert(not make_type_id<int>().is_unsigned());
  static_assert(make_type_id<unsigned int>().is_unsigned());

  static_assert(not make_type_id<char>().is_list());
  static_assert(not make_type_id<int>().is_list());
  static_assert(not make_type_id<float>().is_list());

  static_assert(make_type_id<std::vector<char>>().is_list());
  static_assert(make_type_id<std::vector<int>>().is_list());
  static_assert(make_type_id<std::vector<float>>().is_list());

  static_assert(make_type_id<std::array<unsigned long, 5>>().is_unsigned());
  static_assert(make_type_id<std::array<unsigned long, 5>>().is_list());

  static_assert(make_type_id<char>() < make_type_id<long>());
  static_assert(make_type_id<int>() == make_type_id<int const&>());

  std::function test_fn = [](int a, float b) -> std::tuple<int, float> { return {a, b}; };
  type_ids test_fn_out{make_type_id<int>(), make_type_id<float>()};
  assert(make_output_type_ids<decltype(test_fn)>() == test_fn_out);

  // Now assert all the static_asserts so the coverage script picks them up
  assert(make_type_id<bool>().fundamental() == type_id::builtin::bool_v);
  assert(make_type_id<char>().fundamental() == type_id::builtin::char_v);
  assert(make_type_id<int>().fundamental() == type_id::builtin::int_v);
  assert(make_type_id<double>().fundamental() == type_id::builtin::double_v);
  assert(make_type_id<long>().fundamental() == type_id::builtin::long_v);
  assert(make_type_id<long long>().fundamental() == type_id::builtin::long_long_v);

  assert(not make_type_id<int>().is_unsigned());
  assert(make_type_id<unsigned int>().is_unsigned());

  assert(not make_type_id<char>().is_list());
  assert(not make_type_id<int>().is_list());
  assert(not make_type_id<float>().is_list());

  assert(make_type_id<std::vector<char>>().is_list());
  assert(make_type_id<std::vector<int>>().is_list());
  assert(make_type_id<std::vector<float>>().is_list());

  assert((make_type_id<std::array<unsigned long, 5>>().is_unsigned()));
  assert((make_type_id<std::array<unsigned long, 5>>().is_list()));

  assert(make_type_id<A>().has_children());
  assert(not make_type_id<char>().has_children());
  assert(make_type_id<A>() > make_type_id<bool>());
  assert(make_type_id<char>() < make_type_id<long>());
  assert(make_type_id<int>() == make_type_id<int const&>());

  // Print some type IDs
  fmt::print("void {}\n", make_type_id<void>());
  fmt::print("bool {}\n", make_type_id<bool>());
  fmt::print("char {}\n", make_type_id<char>());
  fmt::print("int {}\n", make_type_id<int>());
  fmt::print("short {}\n", make_type_id<short>());
  fmt::print("long {}\n", make_type_id<long>());
  fmt::print("long long {}\n", make_type_id<long long>());
  fmt::print("float {}\n", make_type_id<float>());
  fmt::print("double {}\n", make_type_id<double>());
  fmt::print("vector<float> {}\n", make_type_id<std::vector<float>>());
  fmt::print("vector<unsigned int> {}\n", make_type_id<std::vector<unsigned int>>());
  fmt::print("A {}\n", make_type_id<A>());
  fmt::print("vector<A> {}\n", make_type_id<std::vector<A>>());

  return 0;
}
