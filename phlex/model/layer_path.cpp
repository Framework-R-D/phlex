#include "layer_path.hpp"

#include "boost/container_hash/hash.hpp"
#include "fmt/format.h"
#include "fmt/ranges.h"

#include <algorithm>
#include <ranges>

using namespace std::literals;
using namespace phlex::experimental::literals;

namespace phlex::experimental {
  layer_path::layer_path(std::string_view path) :
    layer_path_{
      std::from_range,
      path | std::views::split('/') |
        std::views::filter([](auto const& sr) { return not sr.empty(); }) |
        std::views::transform([](auto const& sr) { return identifier(std::string_view(sr)); })}
  {
    if (path.starts_with("/") and not path.starts_with("/job")) {
      throw std::runtime_error(
        fmt::format("A complete layer path must start with '/job'. '{}' does not!", path));
    }
  }

  bool layer_path::empty() const noexcept { return layer_path_.empty(); }

  bool layer_path::complete() const noexcept { return not empty() and layer_path_[0] == "job"_idq; }

  bool layer_path::is_strict_prefix_of(layer_path const& other) const noexcept
  {
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
  bool layer_path::ends_with(identifier const& name) const noexcept
  {
    return not empty() and layer_path_.back() == name;
  }

  std::string layer_path::to_string() const
  {
    return fmt::format("{}{}", complete() ? "/" : "", fmt::join(layer_path_, "/"));
  }
  std::size_t layer_path::hash() const noexcept
  {
    if (empty()) {
      return 0;
    }
    if (layer_path_.size() == 1) {
      return layer_path_[0].hash();
    }
    std::size_t seed = layer_path_[0].hash();
    boost::hash_range(seed, layer_path_.begin() + 1, layer_path_.end());
    return seed;
  }
}
