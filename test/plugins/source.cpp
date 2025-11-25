#include "phlex/source.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/model/product_store.hpp"

#include <ranges>

namespace {
  class number_generator {
  public:
    number_generator(phlex::experimental::configuration const& config) :
      n_{config.get<int>("max_numbers")}
    {
    }

    void next(phlex::experimental::framework_driver& driver) const
    {
      auto job_store = phlex::experimental::product_store::base();
      driver.yield(job_store);

      for (int i : std::views::iota(1, n_ + 1)) {
        auto store = job_store->make_child(i, "event");
        store->add_product("i", i);
        store->add_product("j", -i);
        driver.yield(store);
      }
    }

  private:
    int n_;
  };
}

PHLEX_EXPERIMENTAL_REGISTER_SOURCE(number_generator)
