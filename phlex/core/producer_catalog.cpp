#include "phlex/core/producer_catalog.hpp"
#include "phlex/utilities/bulleted_list.hpp"
#include "phlex/utilities/hashing.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/spdlog.h"
#include <ranges>

namespace phlex::detail {
  std::vector<producer_catalog::named_output_port const*> producer_catalog::find_producers(
    product_selector const& query, phlex::experimental::algorithm_name const& consumer_name) const
  {
    // Will need an update when we have a way to set the current stage name
    if (query.stage.has_value() && query.stage.value() != "CURRENT"_idq) {
      spdlog::debug(
        "{} requires a stage other than the current one. Assuming it comes from a provider.",
        query.to_string());
      return {};
    }
    if (producers_.empty()) {
      spdlog::debug("No producers found. Skipping and assuming {} comes from a provider.",
                    query.to_string());
      return {};
    }
    // Now the only way b == e is if we have a suffix and nothing creates matching products
    auto [b, e] = query.suffix.has_value() ? producers_.equal_range(*query.suffix)
                                           : std::pair{producers_.begin(), producers_.end()};
    if (b == e) {
      spdlog::debug(
        "Failed to find an algorithm that creates {} products. Assuming it comes from a provider",
        query.suffix.value_or("*"_id));
      return {};
    }
    std::vector<named_output_port const*> candidates;

    // Don't really want to copy these fields so we'll use hashes and store a pointer to one copy
    std::map<std::uint64_t, experimental::identifier const*> suffixes;
    if (query.suffix.has_value()) {
      suffixes.emplace(query.suffix->hash(), &*query.suffix);
    }
    std::map<std::uint64_t, experimental::algorithm_name const*> creators;
    std::map<std::uint64_t, type_id const*> types;

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
          candidates.push_back(&producer);
          if (!query.suffix.has_value()) {
            suffixes.emplace(key.hash(), &key);
          }
          creators.emplace(
            phlex::detail::hash(producer.node.plugin().hash(), producer.node.algorithm().hash()),
            &producer.node);
          types.emplace(hash_value(producer.type), &producer.type);
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
    }

    if (candidates.size() > 1ull) {
      static auto const port_to_node =
        [](named_output_port const* p) -> experimental::algorithm_name const& { return p->node; };
      static auto const deref_view =
        std::views::transform([]<typename T>(T const* p) -> T const& { return *p; });
      std::string msg = fmt::format("More than one candidate matches the query {}: \n{}",
                                    query.to_string(),
                                    bulleted_list(std::views::transform(candidates, port_to_node),
                                                  /*indent=*/1));
      if (suffixes.size() == 1 && creators.size() == 1 && types.size() == 1) {
        spdlog::info(msg);
        spdlog::info("This is permitted -- layers may differ");
        return candidates;
      }
      spdlog::error(msg);

      if (suffixes.size() > 1) {
        spdlog::error("Not permitted -- distinguishable by suffix {}",
                      suffixes | std::views::values | deref_view);
      }
      if (creators.size() > 1) {
        spdlog::error("Not permitted -- distinguishable by creator {}",
                      creators | std::views::values |
                        std::views::transform(
                          [](experimental::algorithm_name const* p) { return p->to_string(); }));
      }
      if (types.size() > 1) {
        spdlog::error("Not permitted -- distinguishable by type {}",
                      types | std::views::values | deref_view);
      }
      throw std::runtime_error("Multiple products in candidate set -- see errors");
    }

    return candidates;
  }
}
