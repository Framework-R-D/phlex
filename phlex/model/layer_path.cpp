#include "layer_path.hpp"

#include "phlex/utilities/hashing.hpp"

#include "boost/container_hash/hash.hpp"
#include "fmt/format.h"
#include "fmt/ranges.h"

#include <algorithm>
#include <ranges>

using namespace phlex::experimental::literals;

namespace phlex::experimental {
  layer_path::layer_path(std::string_view path) :
    layer_path_{std::from_range,
                path | std::views::split('/') |
                  std::views::filter([](auto const& sr) { return not sr.empty(); }) |
                  std::views::transform(
                    [](auto const& sr) { return experimental::identifier(std::string_view(sr)); })}
  {
    if (layer_path_.empty()) {
      throw std::runtime_error("Layer paths cannot be empty.");
    }
    if (path.starts_with("/") and not is_complete()) {
      throw std::runtime_error(
        fmt::format("A complete layer path must start with '/job'. '{}' does not!", path));
    }

    validate();
  }

  void layer_path::validate() const
  {
    using namespace literals;
    if (layer_path_.empty()) {
      throw std::runtime_error("Layer paths cannot be empty.");
    }

    // We can use any_of because identifier_query has a function call operator
    if (layer_path_.size() > 1 &&
        std::ranges::any_of(std::span{layer_path_}.subspan(1), "job"_idq)) {
      throw std::runtime_error("Layer paths may only contain 'job' as the first element.");
    }
  }
  bool layer_path::is_complete() const noexcept { return layer_path_[0] == "job"_idq; }

  bool layer_path::is_strict_prefix_of(layer_path const& other) const noexcept
  {
    if (layer_path_.size() >= other.layer_path_.size()) {
      // Optimization, and address Codex observation that a path is not a _strict_ prefix of itself
      return false;
    }
    // starts_with / ends_with aren't supported until libstdc++ *16*
    // return std::ranges::starts_with(other.layer_path_, layer_path_);
    auto const& [it, other_it] = std::ranges::mismatch(layer_path_, other.layer_path_);
    return it == layer_path_.end();
  }

  bool layer_path::ends_with(layer_path const& other) const noexcept
  {
    // starts_with / ends_with aren't supported until libstdc++ *16*
    // return std::ranges::ends_with(layer_path_, other.layer_path_);
    auto rev_layer_path = std::views::reverse(layer_path_);
    auto rev_other_layer_path = std::views::reverse(other.layer_path_);
    auto const& [it, other_it] = std::ranges::mismatch(rev_layer_path, rev_other_layer_path);
    return other_it == rev_other_layer_path.end();
  }

  bool layer_path::ends_with(experimental::identifier const& name) const noexcept
  {
    return layer_path_.back() == name;
  }

  std::string layer_path::to_string() const
  {
    return fmt::format("{}{}", is_complete() ? "/" : "", fmt::join(layer_path_, "/"));
  }

  std::size_t layer_path::hash() const noexcept
  {
    // incomplete paths have an implied "job" root in this calculation
    // Need the experimental:: so it isn't confused for this function
    std::size_t seed =
      is_complete() ? "job"_idq.hash : phlex::detail::hash("job"_idq.hash, layer_path_[0].hash());
    boost::hash_range(seed, layer_path_.begin() + 1, layer_path_.end());
    return seed;
  }

  std::set<std::size_t> layer_path::hashes() const
  {
    std::set<std::size_t> hashes;
    // Add the appropriate first hash
    std::size_t cumulative_hash =
      is_complete() ? "job"_idq.hash : phlex::detail::hash("job"_idq.hash, layer_path_[0].hash());
    hashes.insert(cumulative_hash);
    for (auto const& name : layer_path_ | std::views::drop(1)) {
      cumulative_hash = phlex::detail::hash(cumulative_hash, name.hash());
      hashes.insert(cumulative_hash);
    }
    return hashes;
  }

  std::size_t layer_path::depth() const noexcept { return layer_path_.size(); }
}
