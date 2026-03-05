#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <array>
#include <string>
#include <tuple>

using namespace phlex;
using namespace std::string_literals;

namespace {
  // Provider functions
  int provide_number(data_cell_index const&) { return 3; }
  double provide_temperature(data_cell_index const&) { return 98.5; }
  std::string provide_name(data_cell_index const&) { return "John"; }

  auto no_framework(int num, double temp, std::string const& name)
  {
    return std::make_tuple(num, temp, name);
  }

  auto no_framework_all_refs(int const& num, double const& temp, std::string const& name)
  {
    return std::make_tuple(num, temp, name);
  }

  auto no_framework_all_ptrs(int const* num, double const* temp, std::string const* name)
  {
    return std::make_tuple(*num, *temp, *name);
  }

  auto one_framework_arg(handle<int> num, double temp, std::string const& name)
  {
    return std::make_tuple(*num, temp, name);
  }

  auto all_framework_args(handle<int> const num,
                          handle<double> const temp,
                          handle<std::string> const name)
  {
    return std::make_tuple(*num, *temp, *name);
  }

  void verify_results(int number, double temperature, std::string const& name)
  {
    auto const expected = std::make_tuple(3, 98.5, "John");
    CHECK(std::tie(number, temperature, name) == expected);
  }

}

TEST_CASE("Call non-framework functions", "[programming model]")
{
  std::array const product_names{
    product_query{.creator = "input", .layer = "job", .suffix = "number"},
    product_query{.creator = "input", .layer = "job", .suffix = "temperature"},
    product_query{.creator = "input", .layer = "job", .suffix = "name"}};
  std::array const oproduct_names = {"onumber"s, "otemperature"s, "oname"s};
  std::array const result{"result"s};

  experimental::framework_graph g{data_cell_index::base_ptr()};

  // Register providers
  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "job", .suffix = "number"});
  g.provide("provide_temperature", provide_temperature, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "job", .suffix = "temperature"});
  g.provide("provide_name", provide_name, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "job", .suffix = "name"});

  SECTION("No framework")
  {
    g.transform("no_framework", no_framework)
      .input_family(product_names)
      .output_products(oproduct_names);
  }
  SECTION("No framework, all references")
  {
    g.transform("no_framework_all_refs", no_framework_all_refs)
      .input_family(product_names)
      .output_products(oproduct_names);
  }
  SECTION("No framework, all pointers")
  {
    g.transform("no_framework_all_ptrs", no_framework_all_ptrs)
      .input_family(product_names)
      .output_products(oproduct_names);
  }
  SECTION("One framework argument")
  {
    g.transform("one_framework_arg", one_framework_arg)
      .input_family(product_names)
      .output_products(oproduct_names);
  }
  SECTION("All framework arguments")
  {
    g.transform("all_framework_args", all_framework_args)
      .input_family(product_names)
      .output_products(oproduct_names);
  }

  // The following is invoked for *each* section above
  g.observe("verify_results", verify_results).input_family(product_names);

  g.execute();
}
