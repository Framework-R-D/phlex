#include "phlex/core/declared_provider.hpp"

namespace phlex::experimental {
  declared_provider::declared_provider(algorithm_name name, product_queries output_products) :
    products_consumer{std::move(name), {}, std::move(output_products)}
  {
  }

  declared_provider::~declared_provider() = default;

  void declared_provider::report_cached_stores(stores_t const& stores) const
  {
    if (stores.size() > 0ull) {
      spdlog::warn("Provider {} has {} cached stores.", full_name(), stores.size());
    }
    for (auto const& [hash, store] : stores) {
      if (not store) {
        spdlog::warn("Store with hash {} is null!", hash);
        continue;
      }
      spdlog::debug(" => ID: {} (hash: {})", store->id()->to_string(), hash);
    }
  }
}
