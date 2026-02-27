#include "phlex/core/framework_graph.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <numeric>

using namespace phlex;

namespace types {
  struct Abstract {
    virtual int value() const = 0;
    virtual ~Abstract() = default;
  };
  struct DerivedA : Abstract {
    int value() const override { return 1; }
  };
  struct DerivedB : Abstract {
    int value() const override { return 2; }
  };
}

namespace {
  auto make_derived_as_abstract()
  {
    std::vector<std::unique_ptr<types::Abstract>> vec;
    vec.reserve(2);
    vec.push_back(std::make_unique<types::DerivedA>());
    vec.push_back(std::make_unique<types::DerivedB>());
    return vec;
  }

  int read_abstract(std::vector<std::unique_ptr<types::Abstract>> const& vec)
  {
    return std::transform_reduce(
      vec.begin(), vec.end(), 0, std::plus{}, [](auto const& ptr) -> int { return ptr->value(); });
  }
}

TEST_CASE("Test vector of abstract types")
{
  experimental::layer_generator gen;
  gen.add_layer("event", {"job", 1u, 1u});

  experimental::framework_graph g{driver_for_test(gen)};
  g.provide("provide_thing", [](data_cell_index const&) { return make_derived_as_abstract(); })
    .output_product(product_query{.creator = "dummy", .layer = "event", .suffix = "thing"});
  g.transform("read_thing", read_abstract)
    .input_family(product_query{.creator = "dummy", .layer = "event", .suffix = "thing"})
    .output_products("sum");
  g.observe(
     "verify_sum", [](int sum) { CHECK(sum == 3); }, concurrency::serial)
    .input_family(product_query{.creator = "read_thing", .layer = "event", .suffix = "sum"});
  g.execute();

  CHECK(g.execution_count("provide_thing") == 1);
  CHECK(g.execution_count("read_thing") == 1);
}
