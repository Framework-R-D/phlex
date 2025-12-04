#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <concepts>
#include <string>
#include <vector>

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

  static_assert(std::constructible_from<handle<int>, int, data_cell_index>);
  static_assert(std::constructible_from<handle<int>, int const, data_cell_index>);
  static_assert(std::constructible_from<handle<int>, int const&, data_cell_index>);
  static_assert(not std::constructible_from<handle<int>, double, data_cell_index>);
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

  auto job_data_cell = data_cell_index::base_ptr();
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
  handle const h17{seventeen, data_cell_index::base()};
  handle const h18{eighteen, data_cell_index::base()};
  CHECK(h17 == h17);
  CHECK(h17 != h18);

  auto subrun_6_data_cell = data_cell_index::base_ptr()->make_child(6, "subrun");
  handle const h17sr{seventeen, *subrun_6_data_cell};
  CHECK(*h17 == *h17sr);                                   // Products are the same
  CHECK(h17.data_cell_index() != h17sr.data_cell_index()); // Data cells are not the same
  CHECK(h17 != h17sr);                                     // Therefore handles are not the same
}

TEST_CASE("Handle type conversions (run-time checks)", "[data model]")
{
  int const number{3};
  handle const h{number, data_cell_index::base()};
  CHECK(h.data_cell_index() == data_cell_index::base());

  int const& num_ref = h;
  int const* num_ptr = h;
  CHECK(static_cast<bool>(h));
  CHECK(num_ref == number);
  CHECK(*num_ptr == number);

  Composer const composer{"Elgar"};
  CHECK(handle{composer, data_cell_index::base()}->name == "Elgar");
}
