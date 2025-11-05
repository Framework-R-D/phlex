#include "phlex/model/product_store.hpp"
#include "phlex/source.hpp"

#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

namespace {
  class number_generator {
  public:
    number_generator(phlex::experimental::configuration const& config) :
      n_{config.get<int>("max_numbers")}, coll_{config.get<bool>("as_collection")}
    {
    }

    void next(phlex::experimental::framework_driver& driver) const
    {
      auto job_store = phlex::experimental::product_store::base();
      driver.yield(job_store);

      if (coll_) {
        auto pv = std::make_shared<std::vector<int>>(n_);
        std::iota(pv->begin(), pv->end(), 1);
        auto store = job_store->make_child(0, "event");
        store->add_product("coll_of_i", pv);
        driver.yield(store);
      } else {
        for (int i : std::views::iota(1, n_ + 1)) {
          auto store = job_store->make_child(i, "event");
          store->add_product("i", i);
          store->add_product("j", -i + 1);
          driver.yield(store);
        }
      }
    }

  private:
    int n_;
    bool coll_;
  };
}

PHLEX_EXPERIMENTAL_REGISTER_SOURCE(number_generator)
