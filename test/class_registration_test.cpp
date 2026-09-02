#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <array>
#include <cassert>
#include <string>
#include <tuple>

using namespace std::string_literals;
using namespace phlex;

namespace {
  struct test_struct {
    auto no_framework(int num, double temp, std::string const& name) const
    {
      return std::make_tuple(num, temp, name);
    }

    auto no_framework_all_refs(int const& num, double const& temp, std::string const& name) const
    {
      return std::make_tuple(num, temp, name);
    }

    auto no_framework_all_ptrs(int const* num, double const* temp, std::string const* name) const
    {
      return std::make_tuple(*num, *temp, *name);
    }

    auto one_framework_arg(handle<int> num, double temp, std::string const& name) const
    {
      return std::make_tuple(*num, temp, name);
    }

    auto all_framework_args(handle<int> const num,
                            handle<double> const temp,
                            handle<std::string> const name) const
    {
      return std::make_tuple(*num, *temp, *name);
    }
  };

  void verify_results(int number, double temperature, std::string const& name)
  {
    CHECK(number == 3);
    CHECK(temperature == 98.5);
    CHECK(name == "John");
  }

  auto make_product_selector(experimental::identifier suffix)
  {
    return product_selector{.layer = "job", .suffix = std::move(suffix)};
  }

  auto make_product_selector(experimental::identifier creator, experimental::identifier suffix)
  {
    return product_selector{
      .creator = std::move(creator), .layer = "job", .suffix = std::move(suffix)};
  }

  auto input_products()
  {
    return std::array{make_product_selector("input", "number"),
                      make_product_selector("input", "temperature"),
                      make_product_selector("input", "name")};
  }

  void register_input_providers(auto& graph)
  {
    graph
      .provide(
        "provide_number", [](data_cell_index const&) { return 3; }, concurrency::unlimited)
      .output_product("input", "number", "job");
    graph
      .provide(
        "provide_temperature", [](data_cell_index const&) { return 98.5; }, concurrency::unlimited)
      .output_product("input", "temperature", "job");
    graph
      .provide(
        "provide_name",
        [](data_cell_index const&) { return std::string{"John"}; },
        concurrency::unlimited)
      .output_product("input", "name", "job");
  }

  auto transformed_product_suffixes()
  {
    return std::array<std::string, 3>{
      "transformed_number"s, "transformed_temperature"s, "transformed_name"s};
  }
}

TEST_CASE("Call non-framework functions", "[programming model]")
{
  auto const products = input_products();

  auto g = phlex::detail::framework_graph::with_default_driver();
  register_input_providers(g);

  auto glueball = g.make<test_struct>();
  SECTION("No framework")
  {
    glueball.transform("no_framework", &test_struct::no_framework, concurrency::unlimited)
      .input_family(products)
      .output_product_suffixes(transformed_product_suffixes());
  }
  SECTION("No framework, all references")
  {
    glueball
      .transform(
        "no_framework_all_refs", &test_struct::no_framework_all_refs, concurrency::unlimited)
      .input_family(products)
      .output_product_suffixes(transformed_product_suffixes());
  }
  SECTION("No framework, all pointers")
  {
    glueball
      .transform(
        "no_framework_all_ptrs", &test_struct::no_framework_all_ptrs, concurrency::unlimited)
      .input_family(products)
      .output_product_suffixes(transformed_product_suffixes());
  }
  SECTION("One framework argument")
  {
    glueball.transform("one_framework_arg", &test_struct::one_framework_arg, concurrency::unlimited)
      .input_family(products)
      .output_product_suffixes(transformed_product_suffixes());
  }
  SECTION("All framework arguments")
  {
    glueball
      .transform("all_framework_args", &test_struct::all_framework_args, concurrency::unlimited)
      .input_family(products)
      .output_product_suffixes(transformed_product_suffixes());
  }

  // The following is invoked for *each* section above
  g.observe("verify_results", verify_results)
    .input_family(make_product_selector("transformed_number"),
                  make_product_selector("transformed_temperature"),
                  make_product_selector("transformed_name"));

  g.execute();
}

TEST_CASE("Reuse bound glue object for multiple transforms", "[programming model]")
{
  auto const products = input_products();

  auto g = phlex::detail::framework_graph::with_default_driver();
  register_input_providers(g);

  auto glueball = g.make<test_struct>();
  glueball.transform("first_transform", &test_struct::no_framework, concurrency::unlimited)
    .input_family(products)
    .output_product_suffixes(transformed_product_suffixes());
  glueball.transform("second_transform", &test_struct::no_framework, concurrency::unlimited)
    .input_family(products)
    .output_product_suffixes(transformed_product_suffixes());

  g.observe("verify_first", verify_results, concurrency::unlimited)
    .input_family(make_product_selector("first_transform", "transformed_number"),
                  make_product_selector("first_transform", "transformed_temperature"),
                  make_product_selector("first_transform", "transformed_name"));
  g.observe("verify_second", verify_results, concurrency::unlimited)
    .input_family(make_product_selector("second_transform", "transformed_number"),
                  make_product_selector("second_transform", "transformed_temperature"),
                  make_product_selector("second_transform", "transformed_name"));

  g.execute();

  CHECK(g.execution_count("first_transform") == 1);
  CHECK(g.execution_count("second_transform") == 1);
}
