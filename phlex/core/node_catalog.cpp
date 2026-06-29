#include "phlex/core/node_catalog.hpp"

#include "fmt/format.h"

#include <string>
#include <vector>

using namespace std::string_literals;

namespace phlex::experimental {
  std::vector<products_consumer*> node_catalog::consumers() const
  {
    auto as_product_consumers = [](auto const& nodes) {
      return nodes | std::views::values |
             std::views::transform(
               [](auto const& node) -> products_consumer* { return node.get(); });
    };

    std::vector<products_consumer*> result;
    result.append_range(as_product_consumers(predicates));
    result.append_range(as_product_consumers(observers));
    result.append_range(as_product_consumers(folds));
    result.append_range(as_product_consumers(unfolds));
    result.append_range(as_product_consumers(transforms));
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

  source_vector node_catalog::sources_for(std::vector<std::string> const& keys) const
  {
    source_vector result;
    result.reserve(keys.size());
    for (auto const& key : keys) {
      if (auto src = sources.get(key)) {
        result.push_back(src);
      } else {
        throw std::runtime_error(fmt::format("Unknown source with name: {}", key));
      }
    }
    return result;
  }
}
