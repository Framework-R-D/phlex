#include "phlex/core/declared_observer.hpp"

#include "phlex/core/products_consumer.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/level_id.hpp"
#include "spdlog/spdlog.h"
#include <oneapi/tbb/concurrent_hash_map.h>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  declared_observer::declared_observer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       specified_labels input_products) :
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
