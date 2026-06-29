#ifndef PHLEX_MODEL_LAYER_PATH_HPP
#define PHLEX_MODEL_LAYER_PATH_HPP
#include "phlex/model/identifier.hpp"
#include "phlex/phlex_model_export.hpp"

#include <compare>
#include <concepts>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phlex::experimental {
  class PHLEX_MODEL_EXPORT layer_path {
  public:
    layer_path(std::vector<identifier> const& path) : layer_path_{path} { validate(); }
    layer_path(std::vector<identifier>&& path) : layer_path_{std::move(path)} { validate(); }
    layer_path(std::string_view path);

    template <typename T>
      requires std::constructible_from<std::string_view, T> && (!std::same_as<std::string_view, T>)
    layer_path(T const& path) : layer_path(std::string_view(path))
    {
    }

    auto operator<=>(layer_path const&) const noexcept = default;

    /// Is this path complete (does it start with "job")
    bool is_complete() const noexcept;

    bool is_strict_prefix_of(layer_path const& other) const noexcept;

    bool ends_with(layer_path const& other) const noexcept;

    bool ends_with(identifier const& name) const noexcept;

    std::string to_string() const;

    /// This function assumes incomplete paths have an implicit job root
    std::size_t hash() const noexcept;

    /// Return hash of this path and every parent path
    std::set<std::size_t> hashes() const;

    /// Return the number of layer names in the path
    std::size_t depth() const noexcept;

  private:
    std::vector<identifier> layer_path_;
    void validate() const;
  };

  inline std::string format_as(layer_path const& lp) { return lp.to_string(); }
  inline std::size_t hash_value(layer_path const& lp) { return lp.hash(); }

  // Required by catch2
  inline std::ostream& operator<<(std::ostream& os, layer_path const& lp)
  {
    return os << lp.to_string();
  }
}
#endif // PHLEX_MODEL_LAYER_PATH_HPP
