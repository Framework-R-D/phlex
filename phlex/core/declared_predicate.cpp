#include "phlex/core/declared_predicate.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

namespace phlex::experimental {
  declared_predicate::declared_predicate(algorithm_name name,
                                         std::vector<std::string> predicates,
                                         product_queries input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_predicate::~declared_predicate() = default;

  void declared_predicate::report_cached_results(results_t const& results) const
  {
    if (results.size() > 0ull) {
      spdlog::warn("Filter {} has {} cached results.", full_name(), results.size());
    }
  }
}
