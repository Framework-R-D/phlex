#include "phlex/core/framework_graph.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_all.hpp"

#include <array>
#include <string>
#include <tuple>

using namespace std::string_literals;
using namespace phlex::experimental;

namespace {
  struct A {
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
    auto const expected = std::make_tuple(3, 98.5, "John");
    CHECK(std::tie(number, temperature, name) == expected);
  }
}

TEST_CASE("Call non-framework functions", "[programming model]")
{
  std::array const product_names{
    specified_label{"number"}, specified_label{"temperature"}, specified_label{"name"}};
  std::array const oproduct_names{"onumber"s, "otemperature"s, "oname"s};

  auto store = product_store::base();
  store->add_product("number", 3);
  store->add_product("temperature", 98.5);
  store->add_product("name", std::string{"John"});

  framework_graph g{store};
  auto glueball = g.make<A>();
  SECTION("No framework")
  {
    glueball.with("no_framework", &A::no_framework, concurrency::unlimited)
      .transform(product_names)
      .to(oproduct_names);
  }
  SECTION("No framework, all references")
  {
    glueball.with("no_framework_all_refs", &A::no_framework_all_refs, concurrency::unlimited)
      .transform(product_names)
      .to(oproduct_names);
  }
  SECTION("No framework, all pointers")
  {
    glueball.with("no_framework_all_ptrs", &A::no_framework_all_ptrs, concurrency::unlimited)
      .transform(product_names)
      .to(oproduct_names);
  }
  SECTION("One framework argument")
  {
    glueball.with("one_framework_arg", &A::one_framework_arg, concurrency::unlimited)
      .transform(product_names)
      .to(oproduct_names);
  }
  SECTION("All framework arguments")
  {
    glueball.with("all_framework_args", &A::all_framework_args, concurrency::unlimited)
      .transform(product_names)
      .to(oproduct_names);
  }

  // The following is invoked for *each* section above
  g.with("verify_results", verify_results, concurrency::unlimited).observe(product_names);

  g.execute("class_component_t");
}
