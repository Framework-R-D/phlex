#include "phlex/core/edge_creation_policy.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/spdlog.h"
#include <ranges>

namespace phlex::experimental {
  edge_creation_policy::named_output_port const* edge_creation_policy::find_producer(
    product_query const& query) const
  {
    // TODO: Update later with correct querying
    if (producers_.empty()) {
      spdlog::debug("No producers found. Skipping and assuming {} comes from a provider.", query.to_string());
      return nullptr;
    }
    // Now the only way b == e is if we have a suffix and nothing creates matching products
    auto [b, e] = query.suffix.has_value() ? producers_.equal_range(*query.suffix) : std::pair{producers_.begin(), producers_.end()};
    if (b == e) {
      spdlog::debug(
        "Failed to find an algorithm that creates {} products. Assuming it comes from a provider",
        query.suffix.value_or("*"_id));
      return nullptr;
    }
    std::map<std::string, named_output_port const*> candidates;
    for (auto const& [key, producer] : std::ranges::subrange{b, e}) {
      // TODO: Getting there -- this whole thing needs to be replaced with something
      //       that indexes all the fields from the beginning.
      if (producer.node.plugin() == query.creator || producer.node.algorithm() == query.creator) {
        if (query.type != producer.type) {
          spdlog::debug("Matched ({}) from {} but types don't match (`{}` vs `{}`). Excluding "
                        "from candidate list.",
                        query.to_string(),
                        producer.node.full(),
                        query.type,
                        producer.type);
        } else {
          if (query.type.exact_compare(producer.type)) {
            spdlog::debug("Matched ({}) from {} and types match. Keeping in candidate list.",
                          query.to_string(),
                          producer.node.full());
          } else {
            spdlog::warn("Matched ({}) from {} and types match, but not exactly (produce {} and "
                         "consume {}). Keeping in candidate list!",
                         query.to_string(),
                         producer.node.full(),
                         query.type.exact_name(),
                         producer.type.exact_name());
          }
          candidates.emplace(producer.node.full(), &producer);
        }
      } else {
        spdlog::debug(
          "Creator name mismatch between ({}) and {}", query.to_string(), producer.node.full());
      }
    }

    if (candidates.empty()) {
      spdlog::debug("Cannot identify product matching the query {}. Assuming it comes from a provider.", query.to_string());
      return nullptr;
    }

    if (candidates.size() > 1ull) {
      std::string msg = fmt::format("More than one candidate matches the query {}: \n - {}\n",
                                    query.to_string(),
                                    fmt::join(std::views::keys(candidates), "\n - "));
      throw std::runtime_error(msg);
    }

    return candidates.begin()->second;
  }
}
