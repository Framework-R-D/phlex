#include "phlex/core/edge_creation_policy.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/spdlog.h"
#include <ranges>

namespace phlex::experimental {
  edge_creation_policy::named_output_port const* edge_creation_policy::find_producer(
    product_query const& query) const
  {
    auto const& spec = query.spec;
    auto [b, e] = producers_.equal_range(spec.name());
    if (b == e) {
      spdlog::debug(
        "Failed to find an algorithm that creates {} products. Assuming it comes from a provider",
        spec.name());
      return nullptr;
    }
    std::map<std::string, named_output_port const*> candidates;
    for (auto const& [key, producer] : std::ranges::subrange{b, e}) {
      if (producer.node.match(spec.qualifier())) {
        if (spec.type() != producer.type) {
          spdlog::debug("Matched {} ({}) from {} but types don't match (`{}` vs `{}`). Excluding "
                        "from candidate list.",
                        spec.full(),
                        query.to_string(),
                        producer.node.full(),
                        spec.type(),
                        producer.type);
        } else {
          if (spec.type().exact_compare(producer.type)) {
            spdlog::debug("Matched {} ({}) from {} and types match. Keeping in candidate list.",
                          spec.full(),
                          query.to_string(),
                          producer.node.full());
          } else {
            spdlog::warn("Matched {} ({}) from {} and types match, but not exactly (produce {} and "
                         "consume {}). Keeping in candidate list!",
                         spec.full(),
                         query.to_string(),
                         producer.node.full(),
                         spec.type().exact_name(),
                         producer.type.exact_name());
          }
          candidates.emplace(producer.node.full(), &producer);
        }
      }
    }

    if (candidates.empty()) {
      throw std::runtime_error("Cannot identify product matching the specified label " +
                               spec.full());
    }

    if (candidates.size() > 1ull) {
      std::string msg =
        fmt::format("More than one candidate matches the specification {}: \n - {}\n",
                    spec.full(),
                    fmt::join(std::views::keys(candidates), "\n - "));
      throw std::runtime_error(msg);
    }

    return candidates.begin()->second;
  }
}
