#include "phlex/core/node_catalog.hpp"

namespace phlex::experimental {
  std::size_t node_catalog::execution_counts(std::string const& node_name) const
  {
    // FIXME: Yuck!
    if (auto node = predicates.get(node_name)) {
      return node->num_calls();
    }
    if (auto node = observers.get(node_name)) {
      return node->num_calls();
    }
    if (auto node = folds.get(node_name)) {
      return node->num_calls();
    }
    if (auto node = unfolds.get(node_name)) {
      return node->num_calls();
    }
    if (auto node = transforms.get(node_name)) {
      return node->num_calls();
    }
    return -1u;
  }

  std::size_t node_catalog::product_counts(std::string const& node_name) const
  {
    // FIXME: Yuck!
    if (auto node = folds.get(node_name)) {
      return node->product_count();
    }
    if (auto node = unfolds.get(node_name)) {
      return node->product_count();
    }
    if (auto node = transforms.get(node_name)) {
      return node->product_count();
    }
    return -1u;
  }

}
