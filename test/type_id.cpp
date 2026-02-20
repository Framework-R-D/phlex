#include "phlex/model/type_id.hpp"

#include "catch2/catch_test_macros.hpp"

#include "fmt/format.h"

#include <array>
#include <atomic>
#include <functional>
#include <vector>

using namespace phlex::experimental;

struct A {
  int a;
  int b;
  char c;
  std::atomic<int> d;
};

TEST_CASE("Type ID fundamental types", "[type_id]")
{
  static_assert(make_type_id<bool>().fundamental() == type_id::builtin::bool_v);
  static_assert(make_type_id<char>().fundamental() == type_id::builtin::char_v);
  static_assert(make_type_id<int>().fundamental() == type_id::builtin::int_v);
  static_assert(make_type_id<double>().fundamental() == type_id::builtin::double_v);
  static_assert(make_type_id<long>().fundamental() == type_id::builtin::long_v);
  static_assert(make_type_id<long long>().fundamental() == type_id::builtin::long_long_v);

  CHECK(make_type_id<bool>().fundamental() == type_id::builtin::bool_v);
  CHECK(make_type_id<char>().fundamental() == type_id::builtin::char_v);
  CHECK(make_type_id<int>().fundamental() == type_id::builtin::int_v);
  CHECK(make_type_id<double>().fundamental() == type_id::builtin::double_v);
  CHECK(make_type_id<long>().fundamental() == type_id::builtin::long_v);
  CHECK(make_type_id<long long>().fundamental() == type_id::builtin::long_long_v);
}

TEST_CASE("Type ID unsigned detection", "[type_id]")
{
  static_assert(not make_type_id<int>().is_unsigned());
  static_assert(make_type_id<unsigned int>().is_unsigned());
  static_assert(make_type_id<std::array<unsigned long, 5>>().is_unsigned());

  CHECK_FALSE(make_type_id<int>().is_unsigned());
  CHECK(make_type_id<unsigned int>().is_unsigned());
  CHECK(make_type_id<std::array<unsigned long, 5>>().is_unsigned());
}

TEST_CASE("Type ID list detection", "[type_id]")
{
  static_assert(not make_type_id<char>().is_list());
  static_assert(not make_type_id<int>().is_list());
  static_assert(not make_type_id<float>().is_list());
  static_assert(make_type_id<std::vector<char>>().is_list());
  static_assert(make_type_id<std::vector<int>>().is_list());
  static_assert(make_type_id<std::vector<float>>().is_list());
  static_assert(make_type_id<std::array<unsigned long, 5>>().is_list());

  CHECK_FALSE(make_type_id<char>().is_list());
  CHECK_FALSE(make_type_id<int>().is_list());
  CHECK_FALSE(make_type_id<float>().is_list());
  CHECK(make_type_id<std::vector<char>>().is_list());
  CHECK(make_type_id<std::vector<int>>().is_list());
  CHECK(make_type_id<std::vector<float>>().is_list());
  CHECK(make_type_id<std::array<unsigned long, 5>>().is_list());
}

TEST_CASE("Type ID equality and comparison", "[type_id]")
{
  static_assert(make_type_id<char>() != make_type_id<long>());
  static_assert(make_type_id<int>() == make_type_id<int const&>());

  CHECK(make_type_id<char>() != make_type_id<long>());
  CHECK(make_type_id<int>() == make_type_id<int const&>());
  CHECK(make_type_id<A>() > make_type_id<bool>());
}

TEST_CASE("Type ID children detection", "[type_id]")
{
  CHECK(make_type_id<A>().has_children());
  CHECK_FALSE(make_type_id<char>().has_children());
}

TEST_CASE("Type ID output type deduction", "[type_id]")
{
  std::function test_fn = [](int a, float b) -> std::tuple<int, float> { return {a, b}; };
  type_ids test_fn_out{make_type_id<int>(), make_type_id<float>()};
  CHECK(make_output_type_ids<decltype(test_fn)>() == test_fn_out);
}

TEST_CASE("Type ID string formatting", "[type_id]")
{
  CHECK(fmt::format("{}", make_type_id<void>()) == "void");
  CHECK(fmt::format("{}", make_type_id<bool>()) == "bool");
  CHECK(fmt::format("{}", make_type_id<char>()) == "char");
  CHECK(fmt::format("{}", make_type_id<int>()) == "int");
  CHECK(fmt::format("{}", make_type_id<short>()) == "short");
  CHECK(fmt::format("{}", make_type_id<long>()) == "long");
  CHECK(fmt::format("{}", make_type_id<long long>()) == "long long");
  CHECK(fmt::format("{}", make_type_id<float>()) == "float");
  CHECK(fmt::format("{}", make_type_id<double>()) == "double");
  CHECK(fmt::format("{}", make_type_id<long double>()) == "long double");
  CHECK(fmt::format("{}", make_type_id<std::vector<float>>()) == "LIST float");
  CHECK(fmt::format("{}", make_type_id<std::vector<unsigned int>>()) == "LIST unsigned int");
  CHECK(fmt::format("{}", make_type_id<A>()) == "STRUCT {int, int, char, int}");
  CHECK(fmt::format("{}", make_type_id<std::vector<A>>()) == "LIST STRUCT {int, int, char, int}");
}
