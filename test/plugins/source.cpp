#include "phlex/source.hpp"
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
      auto job_index = phlex::experimental::data_cell_index::base_ptr();
      driver.yield(job_index);

      for (int i : std::views::iota(1, n_ + 1)) {
        auto index = job_index->make_child(i, "event");
        driver.yield(index);
      }
    }

  private:
    int n_;
  };
}

PHLEX_EXPERIMENTAL_REGISTER_SOURCE(number_generator)
