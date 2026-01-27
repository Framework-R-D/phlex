#include "phlex/core/edge_creation_policy.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <ranges>
#include <set>

namespace phlex::experimental {
  edge_creation_policy::named_output_port const* edge_creation_policy::find_producer(
    product_query const& query) const
  {
    auto [creator_b, creator_e] = creator_db_.equal_range(query.creator_hash());
    if (creator_b == creator_e) {
      spdlog::debug("Found no matching creators for {}. Assuming it comes from a provider.",
                    query.to_string());
      return nullptr;
    }
    std::set matching_creator = std::ranges::subrange(creator_b, creator_e) | std::views::values |
                                std::ranges::to<std::set>();

    auto [type_b, type_e] = type_db_.equal_range(query.type_hash());
    if (type_b == type_e) {
      throw std::runtime_error(fmt::format(
        "Found no matching types for {} (require {})", query.to_string(), query.type()));
    }
    std::set matching_type =
      std::ranges::subrange(type_b, type_e) | std::views::values | std::ranges::to<std::set>();

    std::set<named_output_port const*> creator_and_type{};
    std::ranges::set_intersection(
      matching_creator, matching_type, std::inserter(creator_and_type, creator_and_type.begin()));
    if (creator_and_type.empty()) {
      throw std::runtime_error(
        fmt::format("Found no creator + type match for {} (required type {})",
                    query.to_string(),
                    query.type()));
    }

    std::set<named_output_port const*> all_matched;
    if (query.suffix()) {
      for (auto const* port : creator_and_type) {
        if (port->suffix == query.suffix()) {
          all_matched.insert(port);
        }
      }
      if (all_matched.empty()) {
        throw std::runtime_error(
          fmt::format("Of {} creator + type matches for {}, found 0 suffix matches",
                      creator_and_type.size(),
                      query.to_string()));
      }
    } else {
      all_matched = std::move(creator_and_type);
    }
    if (all_matched.size() > 1) {
      std::string msg =
        fmt::format("Found {} duplicate matches for {}", all_matched.size(), query.to_string());
      throw std::runtime_error(msg);
    }

    return *all_matched.begin();
  }

  std::uint64_t edge_creation_policy::hash_string(std::string const& str)
  {
    using namespace boost::hash2;
    xxhash_64 h;
    hash_append(h, {}, str);
    return h.result();
  }
}
