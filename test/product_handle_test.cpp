#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"

#include <catch2/catch_test_macros.hpp>
#include <gsl/pointers>

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace phlex;
using namespace phlex::experimental::literals;
using spec_t = experimental::product_specification;

namespace {
  struct composer {
    std::string name;
  };

  template <typename T>
  auto make_handle(T const& product, data_cell_index const& index, spec_t const& spec)
  {
    static auto stage = "test_stage"_id;
    return handle{gsl::make_not_null(&product),
                  gsl::make_not_null(&index),
                  gsl::make_not_null(&spec),
                  gsl::not_null<experimental::identifier const*>{&stage}};
  }
}

TEST_CASE("Handle type conversions (compile-time checks)", "[data model]")
{
  using detail::internal::handle_value_type;
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
  spec_t const two_spec{"two"};
  spec_t const four_spec{"four"};

  auto job_data_cell = data_cell_index::job();
  auto subrun_6_data_cell = job_data_cell->make_child("subrun", 6);

  auto h2 = make_handle(two, *job_data_cell, two_spec);
  auto h4 = make_handle(four, *subrun_6_data_cell, four_spec);
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
  spec_t const seventeen_spec{"seventeen"};
  spec_t const eighteen_spec{"eighteen"};
  auto const h17 = make_handle(seventeen, *data_cell_index::job(), seventeen_spec);
  auto const h18 = make_handle(eighteen, *data_cell_index::job(), eighteen_spec);
  CHECK(h17 == h17);
  CHECK(h17 != h18);

  auto subrun_6_data_cell = data_cell_index::job()->make_child("subrun", 6);
  auto const h17sr = make_handle(seventeen, *subrun_6_data_cell, seventeen_spec);
  CHECK(*h17 == *h17sr);                                   // Products are the same
  CHECK(h17.data_cell_index() != h17sr.data_cell_index()); // Data cells are not the same
  CHECK(h17 != h17sr);                                     // Therefore handles are not the same
}

TEST_CASE("Handle type conversions (run-time checks)", "[data model]")
{
  int const number{3};
  spec_t const spec{"number"};
  auto const h = make_handle(number, *data_cell_index::job(), spec);
  CHECK(h.data_cell_index() == *data_cell_index::job());

  int const& num_ref = h;
  int const* num_ptr = h;
  CHECK(static_cast<bool>(h));
  CHECK(num_ref == number);
  CHECK(*num_ptr == number);

  composer const elgar{"Elgar"};
  spec_t const composer_spec{"composer"};
  auto const h2 = make_handle(elgar, *data_cell_index::job(), composer_spec);
  CHECK(h2->name == "Elgar");
}

TEST_CASE("Retrieve product specification from handle", "[data model]")
{
  int const number{3};
  spec_t const spec{"creator/three"};

  auto const h = make_handle(number, *data_cell_index::job(), spec);
  CHECK(h.creator().algorithm == "creator");
  CHECK(h.suffix() == "three");
  CHECK(h.layer() == "job");
  CHECK(h.stage() == "test_stage");
}

TEST_CASE("Handle metadata accessors return textual views", "[data model]")
{
  using handle_t = handle<int>;
  STATIC_CHECK(std::is_same_v<decltype(std::declval<handle_t>().suffix()), std::string_view>);
  STATIC_CHECK(std::is_same_v<decltype(std::declval<handle_t>().layer()), std::string_view>);
  STATIC_CHECK(std::is_same_v<decltype(std::declval<handle_t>().stage()), std::string_view>);
  STATIC_CHECK(
    std::is_same_v<decltype(std::declval<handle_t>().creator().plugin), std::string_view>);
  STATIC_CHECK(
    std::is_same_v<decltype(std::declval<handle_t>().creator().algorithm), std::string_view>);
}

TEST_CASE("Handle suffix views distinguish empty and named suffixes", "[data model]")
{
  int const number{3};
  auto const job = data_cell_index::job();

  SECTION("Explicit empty suffix")
  {
    spec_t const spec{"creator/"};
    auto const h = make_handle(number, *job, spec);
    CHECK(h.suffix().empty());
    CHECK(h.suffix() == "");
    CHECK(h.creator().algorithm == "creator");
  }

  SECTION("Default specification has an empty suffix")
  {
    spec_t const spec;
    auto const h = make_handle(number, *job, spec);
    CHECK(h.suffix().empty());
  }

  SECTION("Named suffix")
  {
    spec_t const spec{"creator/hits"};
    auto const h = make_handle(number, *job, spec);
    CHECK_FALSE(h.suffix().empty());
    CHECK(h.suffix() == "hits");
  }
}

TEST_CASE("Handle creator views for absent and present creators", "[data model]")
{
  int const number{3};
  auto const job = data_cell_index::job();

  SECTION("Absent creator with a named suffix")
  {
    spec_t const spec{"hits"};
    REQUIRE_FALSE(spec.has_creator());
    auto const h = make_handle(number, *job, spec);
    CHECK(h.creator().plugin.empty());
    CHECK(h.creator().algorithm.empty());
    CHECK(h.suffix() == "hits");
  }

  SECTION("Absent creator with an empty suffix")
  {
    spec_t const spec{"/"};
    REQUIRE_FALSE(spec.has_creator());
    auto const h = make_handle(number, *job, spec);
    CHECK(h.creator().plugin.empty());
    CHECK(h.creator().algorithm.empty());
    CHECK(h.suffix().empty());
  }

  SECTION("Unqualified creator has an empty plugin view")
  {
    spec_t const spec{"find_hits/hits"};
    REQUIRE(spec.has_creator());
    auto const h = make_handle(number, *job, spec);
    CHECK(h.creator().plugin.empty());
    CHECK(h.creator().algorithm == "find_hits");
  }

  SECTION("Qualified creator exposes both components")
  {
    spec_t const spec{"reco:find_hits/hits"};
    REQUIRE(spec.has_creator());
    auto const h = make_handle(number, *job, spec);
    CHECK(h.creator().plugin == "reco");
    CHECK(h.creator().algorithm == "find_hits");
    CHECK(h.suffix() == "hits");
  }
}

TEST_CASE("Handle views are borrowed from the owning specification", "[data model]")
{
  int const number{3};
  auto const job = data_cell_index::job();
  spec_t const spec{"reco:find_hits/hits"};
  auto const h = make_handle(number, *job, spec);

  // The views alias the text stored in the specification's values, rather than copies.
  auto const creator = h.creator();
  CHECK(creator.plugin.data() == spec.plugin().text().data());
  CHECK(creator.algorithm.data() == std::string_view(*spec.algorithm()).data());
  CHECK(h.suffix().data() == spec.suffix().text().data());

  // Repeated calls refer to the same borrowed storage.
  CHECK(h.suffix().data() == h.suffix().data());
  CHECK(h.creator().plugin.data() == creator.plugin.data());

  // A copy of the handle borrows from the same specification.
  auto const copy = h;
  CHECK(copy.suffix().data() == spec.suffix().text().data());
  CHECK(copy.creator().algorithm.data() == creator.algorithm.data());
}

TEST_CASE("Retrieve stage from handle", "[data model]")
{
  int const number{3};
  spec_t const spec("creator/three");

  auto const h = make_handle(number, *data_cell_index::job(), spec);
  CHECK(h.stage() == "test_stage");
}
