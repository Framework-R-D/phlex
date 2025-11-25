#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/products.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <concepts>
#include <string>

using namespace phlex::experimental;

namespace {
  struct Composer {
    std::string name;
  };
}

TEST_CASE("Handle type conversions (compile-time checks)", "[data model]")
{
  using detail::handle_value_type;
  static_assert(std::same_as<handle_value_type<int>, int>);
  static_assert(std::same_as<handle_value_type<int const>, int>);
  static_assert(std::same_as<handle_value_type<int const&>, int>);
  static_assert(std::same_as<handle_value_type<int const*>, int>);
  static_assert(std::same_as<handle_value_type<handle<int>>, int>);
}

TEST_CASE("Can only construct handles with compatible types (compile-time checks)", "[data model]")
{
  static_assert(std::constructible_from<handle<int>, handle<int> const&>); // Copies
  static_assert(std::constructible_from<handle<int>, handle<int>&&>);      // Moves
  static_assert(not std::constructible_from<handle<int>, handle<double>>);

  static_assert(std::constructible_from<handle<int>, int, level_id>);
  static_assert(std::constructible_from<handle<int>, int const, level_id>);
  static_assert(std::constructible_from<handle<int>, int const&, level_id>);
  static_assert(not std::constructible_from<handle<int>, double, level_id>);
}

TEST_CASE("Can only assign handles with compatible types (compile-time checks)", "[data model]")
{
  static_assert(std::assignable_from<handle<int>&, handle<int> const&>); // Copies
  static_assert(std::assignable_from<handle<int>&, handle<int>&&>);      // Moves
  static_assert(not std::assignable_from<handle<int>&, handle<double> const&>);
}

TEST_CASE("Handle copies and moves", "[data model]")
{
  int const two{2};
  int const four{4};

  auto job_data_cell = level_id::base_ptr();
  auto subrun_6_data_cell = job_data_cell->make_child(6, "subrun");

  handle h2{two, *job_data_cell};
  handle h4{four, *subrun_6_data_cell};
  CHECK(h2 != h4);

  CHECK(handle{h2} == h2);
  h2 = h4;
  CHECK(h2 == h4);
  CHECK(*h2 == 4);

  handle h3 = std::move(h4);
  CHECK(*h3 == 4);

  h4 = h2;
  CHECK(h2 == h4);
  CHECK(*h4 == 4);

  h4 = std::move(h3);
  CHECK(*h4 == 4);
}

TEST_CASE("Handle comparisons", "[data model]")
{
  int const seventeen{17};
  int const eighteen{18};
  handle const h17{seventeen, level_id::base()};
  handle const h18{eighteen, level_id::base()};
  CHECK(h17 == h17);
  CHECK(h17 != h18);

  auto subrun_6_data_cell = level_id::base_ptr()->make_child(6, "subrun");
  handle const h17sr{seventeen, *subrun_6_data_cell};
  CHECK(*h17 == *h17sr);                     // Products are the same
  CHECK(h17.level_id() != h17sr.level_id()); // Levels are not the same
  CHECK(h17 != h17sr);                       // Therefore handles are not the same
}

TEST_CASE("Handle type conversions (run-time checks)", "[data model]")
{
  int const number{3};
  handle const h{number, level_id::base()};
  CHECK(h.level_id() == level_id::base());

  int const& num_ref = h;
  int const* num_ptr = h;
  CHECK(static_cast<bool>(h));
  CHECK(num_ref == number);
  CHECK(*num_ptr == number);

  Composer const composer{"Elgar"};
  CHECK(handle{composer, level_id::base()}->name == "Elgar");
}
