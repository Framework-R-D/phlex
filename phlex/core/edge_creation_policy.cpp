#include "phlex/core/edge_creation_policy.hpp"

#include "spdlog/spdlog.h"
#include <ranges>
#include <sstream>

namespace phlex::experimental {
  edge_creation_policy::named_output_port const* edge_creation_policy::find_producer(
    product_specification const& specified_product_name) const
  {
    auto [b, e] = producers_.equal_range(specified_product_name.name());
    if (b == e) {
      return nullptr;
    }
    std::map<std::string, named_output_port const*> candidates;
    for (auto const& [key, producer] : std::ranges::subrange{b, e}) {
      if (producer.node.match(specified_product_name.qualifier())) {
        if (specified_product_name.type() != producer.type) {
          spdlog::warn("Matched {} from {} but types don't match (`{}` vs `{}`)",
                       specified_product_name.full(),
                       producer.node.full(),
                       specified_product_name.type(),
                       producer.type);
        } else {
          spdlog::debug("Matched {} from {} and types match (`{}` vs `{}`)",
                        specified_product_name.full(),
                        producer.node.full(),
                        specified_product_name.type(),
                        producer.type);
        }
        candidates.emplace(producer.node.full(), &producer);
      }
    }

    if (candidates.empty()) {
      throw std::runtime_error("Cannot identify product matching the specified label " +
                               specified_product_name.full());
    }

    if (candidates.size() > 1ull) {
      std::ostringstream msg;
      msg << "More than one candidate matches the specified label " << specified_product_name.full()
          << ":";
      for (auto const& key : candidates | std::views::keys) {
        msg << "\n  - " << key;
      }
      msg << '\n';
      throw std::runtime_error(msg.str());
    }

    return candidates.begin()->second;
  }
}
