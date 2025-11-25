#include "phlex/core/framework_graph.hpp"

#include "catch2/catch_test_macros.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/model/product_store.hpp"

#include <functional>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

using namespace phlex::experimental;

namespace types {
  struct Abstract {
    [[nodiscard]] virtual auto value() const -> int = 0;
    virtual ~Abstract() = default;
  };
  struct DerivedA : Abstract {
    [[nodiscard]] auto value() const -> int override { return 1; }
  };
  struct DerivedB : Abstract {
    [[nodiscard]] auto value() const -> int override { return 2; }
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

  auto read_abstract(std::vector<std::unique_ptr<types::Abstract>> const& vec) -> int
  {
    return std::transform_reduce(
      vec.begin(), vec.end(), 0, std::plus{}, [](auto const& ptr) -> int { return ptr->value(); });
  }

  class source {
  public:
    explicit source(unsigned const max_n) : max_{max_n} {}

    void operator()(framework_driver& driver)
    {
      auto job_store = phlex::experimental::product_store::base();
      driver.yield(job_store);

      for (unsigned int i : std::views::iota(1u, max_ + 1)) {
        auto store = job_store->make_child(i, "event");
        store->add_product("thing", make_derived_as_abstract());
        driver.yield(store);
      }
    }

  private:
    unsigned const max_;
  };

}

TEST_CASE("Test vector of abstract types")
{
  framework_graph g{source{1u}};
  g.transform("read_thing", read_abstract).input_family("thing").output_products("sum");
  g.observe(
     "verify_sum", [](int sum) -> void { CHECK(sum == 3); }, concurrency::serial)
    .input_family("sum");
  g.execute();

  CHECK(g.execution_counts("read_thing") == 1);
}
