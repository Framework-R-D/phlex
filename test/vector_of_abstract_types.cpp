#include "phlex/core/framework_graph.hpp"

#include "catch2/catch_test_macros.hpp"

#include <numeric>

using namespace phlex::experimental;

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

  class source {
  public:
    explicit source(unsigned const max_n) : max_{max_n} {}

    void operator()(framework_driver& driver)
    {
      auto job_index = phlex::experimental::data_cell_index::base_ptr();
      driver.yield(job_index);

      for (unsigned int i : std::views::iota(1u, max_ + 1)) {
        driver.yield(job_index->make_child(i, "event"));
      }
    }

  private:
    unsigned const max_;
  };

}

TEST_CASE("Test vector of abstract types")
{
  framework_graph g{source{1u}};
  g.provide("provide_thing", [](data_cell_index const&) { return make_derived_as_abstract(); })
    .output_product("thing"_in("event"));
  g.transform("read_thing", read_abstract).input_family("thing"_in("event")).output_products("sum");
  g.observe(
     "verify_sum", [](int sum) { CHECK(sum == 3); }, concurrency::serial)
    .input_family("sum"_in("event"));
  g.execute();

  CHECK(g.execution_counts("read_thing") == 1);
}
