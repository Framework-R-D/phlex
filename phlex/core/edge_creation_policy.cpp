#include "phlex/core/edge_creation_policy.hpp"
#include "phlex/model/qualified_name.hpp"

#include <map>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>

namespace phlex::experimental {
  auto edge_creation_policy::find_producer(
    qualified_name const& specified_product_name) const -> edge_creation_policy::named_output_port const*
  {
    auto [b, e] = producers_.equal_range(specified_product_name.name());
    if (b == e) {
      return nullptr;
    }
    std::map<std::string, named_output_port const*> candidates;
    for (auto const& [key, producer] : std::ranges::subrange{b, e}) {
      if (producer.node.match(specified_product_name.qualifier())) {
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
