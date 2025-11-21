#include "phlex/core/declared_observer.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

namespace phlex::experimental {
  declared_observer::declared_observer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_queries input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_observer::~declared_observer() = default;

  void declared_observer::report_cached_hashes(
    tbb::concurrent_hash_map<level_id::hash_type, bool> const& hashes) const
  {
    if (hashes.size() > 0ull) {
      spdlog::warn("Monitor {} has {} cached hashes.", full_name(), hashes.size());
    }
    for (auto const& [id, _] : hashes) {
      spdlog::debug(" => ID: {}", id);
    }
  }
}
