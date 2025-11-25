#include "phlex/core/framework_graph.hpp"
#include "phlex/core/fwd.hpp" // FIXME:  If framework_driver is public API, it should live in an easy-to-find place.
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <functional>
#include <memory>
#include <numeric>
#include <vector>

using namespace phlex::experimental;

namespace types {
  struct abstract {
    virtual int value() const = 0;
    virtual ~abstract() = default;
  };
  struct derived_a : abstract {
    int value() const override { return 1; }
  };
  struct derived_b : abstract {
    int value() const override { return 2; }
  };
}

namespace {
  auto make_derived_as_abstract()
  {
    std::vector<std::unique_ptr<types::abstract>> vec;
    vec.reserve(2);
    vec.push_back(std::make_unique<types::derived_a>());
    vec.push_back(std::make_unique<types::derived_b>());
    return vec;
  }

  int read_abstract(std::vector<std::unique_ptr<types::abstract>> const& vec)
  {
    return std::transform_reduce(
      vec.begin(), vec.end(), 0, std::plus{}, [](auto const& ptr) -> int { return ptr->value(); });
  }

  class source {
  public:
    explicit source(unsigned const max_n) : max_{max_n} {}

    void operator()(framework_driver& driver) const
    {
      auto job_store = phlex::experimental::product_store::base();
      driver.yield(job_store);

      for (unsigned int const i : std::views::iota(1u, max_ + 1)) {
        auto store = job_store->make_child(i, "event");
        store->add_product("thing", make_derived_as_abstract());
        driver.yield(store);
      }
    }

  private:
    unsigned max_;
  };

}

TEST_CASE("Test vector of abstract types")
{
  framework_graph g{source{1u}};
  g.transform("read_thing", read_abstract).input_family("thing").output_products("sum");
  g.observe(
     "verify_sum", [](int sum) { CHECK(sum == 3); }, concurrency::serial)
    .input_family("sum");
  g.execute();

  CHECK(g.execution_counts("read_thing") == 1);
}
