#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/level_counter.hpp"

#include "phlex/model/product_store.hpp"
#include "phlex/model/products.hpp"
#include "spdlog/spdlog.h"
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {

  generator::generator(product_store_const_ptr const& parent,
                       std::string node_name,
                       std::string const& new_level_name) :
    parent_{std::const_pointer_cast<product_store>(parent)},
    node_name_{std::move(node_name)},
    new_level_name_{new_level_name}
  {
  }

  auto generator::make_child(std::size_t const i, products new_products) -> product_store_const_ptr
  {
    auto child = parent_->make_child(i, new_level_name_, node_name_, std::move(new_products));
    ++child_counts_[child->id()->level_hash()];
    return child;
  }

  auto generator::flush_store() const -> product_store_const_ptr
  {
    auto result = parent_->make_flush();
    if (not child_counts_.empty()) {
      result->add_product("[flush]",
                          std::make_shared<flush_counts const>(std::move(child_counts_)));
    }
    return result;
  }

  declared_unfold::declared_unfold(algorithm_name name,
                                   std::vector<std::string> predicates,
                                   specified_labels input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_unfold::~declared_unfold() = default;

  void declared_unfold::report_cached_stores(stores_t const& stores) const
  {
    if (stores.size() > 0ull) {
      spdlog::warn("Unfold {} has {} cached stores.", full_name(), stores.size());
    }
    for (auto const& [hash, store] : stores) {
      spdlog::debug(" => ID: {} (hash: {})", store->id()->to_string(), hash);
    }
  }
}
