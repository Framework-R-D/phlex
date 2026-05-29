#include "phlex/core/node_catalog.hpp"

#include <string>

using namespace std::string_literals;

namespace phlex::experimental {
  std::vector<products_consumer*> node_catalog::consumers()
  {
    std::vector<products_consumer*> result;
    for (auto const& [_, node] : predicates) {
      result.push_back(node.get());
    }
    for (auto const& [_, node] : observers) {
      result.push_back(node.get());
    }
    for (auto const& [_, node] : folds) {
      result.push_back(node.get());
    }
    for (auto const& [_, node] : unfolds) {
      result.push_back(node.get());
    }
    for (auto const& [_, node] : transforms) {
      result.push_back(node.get());
    }
    return result;
  }

  std::size_t node_catalog::execution_count(std::string const& node_name) const
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
    if (auto node = providers.get(node_name)) {
      return node->num_calls();
    }
    if (auto node = outputs.get(node_name)) {
      return node->num_calls();
    }
    throw std::runtime_error("Unknown node type with name: "s + node_name);
  }

  producer_catalog node_catalog::producers() const
  {
    return producer_catalog{transforms, folds, unfolds};
  }
}
