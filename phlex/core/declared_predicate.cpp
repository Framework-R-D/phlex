#include "phlex/core/declared_predicate.hpp"

#include "phlex/core/products_consumer.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "spdlog/spdlog.h"
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  declared_predicate::declared_predicate(algorithm_name name,
                                         std::vector<std::string> predicates,
                                         specified_labels input_products) :
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
