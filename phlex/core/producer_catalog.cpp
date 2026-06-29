#include "phlex/core/producer_catalog.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/spdlog.h"
#include <ranges>

namespace phlex::experimental {
  producer_catalog::named_output_port const* producer_catalog::find_producer(
    product_selector const& query, algorithm_name const& consumer_name) const
  {
    if (producers_.empty()) {
      spdlog::debug("No producers found. Skipping and assuming {} comes from a provider.",
                    query.to_string());
      return nullptr;
    }
    // Now the only way b == e is if we have a suffix and nothing creates matching products
    auto [b, e] = query.suffix.has_value() ? producers_.equal_range(*query.suffix)
                                           : std::pair{producers_.begin(), producers_.end()};
    if (b == e) {
      spdlog::debug(
        "Failed to find an algorithm that creates {} products. Assuming it comes from a provider",
        query.suffix.value_or("*"_id));
      return nullptr;
    }
    std::map<std::string, named_output_port const*> candidates;
    for (auto const& [key, producer] : std::ranges::subrange{b, e}) {
      // Prevent self-edges
      if (producer.node == consumer_name) {
        spdlog::debug(
          "Skipping self-edge if {} matched {}", query.to_string(), producer.node.to_string());
        continue;
      }
      spdlog::debug("Checking product made by {} against input required by {}",
                    producer.node.to_string(),
                    consumer_name.to_string());

      // TODO: Getting there -- this whole thing needs to be replaced with something
      //       that indexes all the fields from the beginning.
      if (query.creator_match(producer.node)) {
        if (query.type != producer.type) {
          spdlog::debug("Matched ({}) from {} but types don't match (`{}` vs `{}`). Excluding "
                        "from candidate list.",
                        query.to_string(),
                        producer.node.to_string(),
                        query.type,
                        producer.type);
        } else {
          if (query.type.exact_compare(producer.type)) {
            spdlog::debug("Matched ({}) from {} and types match. Keeping in candidate list.",
                          query.to_string(),
                          producer.node.to_string());
          } else {
            spdlog::warn("Matched ({}) from {} and types match, but not exactly (produce {} and "
                         "consume {}). Keeping in candidate list!",
                         query.to_string(),
                         producer.node.to_string(),
                         query.type.exact_name(),
                         producer.type.exact_name());
          }
          candidates.emplace(producer.node.to_string(), &producer);
        }
      } else {
        spdlog::debug("Creator name mismatch between ({}) and {}",
                      query.to_string(),
                      producer.node.to_string());
      }
    }

    if (candidates.empty()) {
      spdlog::debug(
        "Cannot identify product matching the query {}. Assuming it comes from a provider.",
        query.to_string());
      return nullptr;
    }

    if (candidates.size() > 1ull) {
      std::string msg = fmt::format("More than one candidate matches the query {}: \n{}\n",
                                    query.to_string(),
                                    phlex::detail::bulleted_list(std::views::keys(candidates),
                                                                 /*indent=*/1));
      throw std::runtime_error(msg);
    }

    return candidates.begin()->second;
  }
}
