#include "phlex/core/declared_provider.hpp"

namespace phlex::experimental {
  declared_provider::declared_provider(algorithm_name name, product_query output_product) :
    name_{std::move(name)}, output_product_{std::move(output_product)}
  {
  }

  declared_provider::~declared_provider() = default;

  std::string declared_provider::full_name() const { return name_.full(); }

  product_query const& declared_provider::output_product() const noexcept
  {
    return output_product_;
  }

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
