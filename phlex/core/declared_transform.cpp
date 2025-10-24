#include "phlex/core/declared_transform.hpp"

#include "phlex/core/products_consumer.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "spdlog/spdlog.h"
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  declared_transform::declared_transform(algorithm_name name,
                                         std::vector<std::string> predicates,
                                         specified_labels input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_transform::~declared_transform() = default;

  void declared_transform::report_cached_stores(stores_t const& stores) const
  {
    if (stores.size() > 0ull) {
      spdlog::warn("Transform {} has {} cached stores.", full_name(), stores.size());
    }
    for (auto const& [hash, store] : stores) {
      spdlog::debug(" => ID: {} (hash: {})", store->id()->to_string(), hash);
    }
  }
}
