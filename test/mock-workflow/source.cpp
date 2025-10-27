#include "phlex/source.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/model/product_store.hpp"

#include "spdlog/spdlog.h"

#include <cstddef>
#include <ranges>

namespace phlex::experimental::test {
  class source {
  public:
    source(phlex::experimental::configuration const& config) :
      max_{config.get<std::size_t>("n_events")}
    {
      spdlog::info("Processing {} events", max_);
    }

    void next(framework_driver& driver) const
    {
      auto job_store = phlex::experimental::product_store::base();
      driver.yield(job_store);

      for (std::size_t i : std::views::iota(0u, max_)) {
        if (max_ > 10 && (i % (max_ / 10) == 0)) {
          spdlog::debug("Reached {} events", i);
        }

        auto store = job_store->make_child(i, "event");
        store->add_product("id", *store->id());
        driver.yield(store);
      }
    }

  private:
    std::size_t max_;
  };
}

PHLEX_EXPERIMENTAL_REGISTER_SOURCE(phlex::experimental::test::source)
