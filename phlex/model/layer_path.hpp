#ifndef PHLEX_MODEL_LAYER_PATH_HPP
#define PHLEX_MODEL_LAYER_PATH_HPP
#include "phlex/model/identifier.hpp"
#include "phlex/phlex_model_export.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace phlex::experimental {
  class PHLEX_MODEL_EXPORT layer_path {
  public:
    layer_path() = default;
    layer_path(std::vector<identifier> const& path) : layer_path_{path} {}
    layer_path(std::vector<identifier>&& path) : layer_path_{std::move(path)} {}
    layer_path(std::string_view path);

    auto operator<=>(layer_path const&) const noexcept = default;

    bool empty() const noexcept;

    /// Is this path complete (does it start with "job")
    bool complete() const noexcept;

    /// Is this path a strict prefix of other
    bool is_strict_prefix_of(layer_path const& other) const noexcept;

    /// Does this path end with other
    bool ends_with(layer_path const& other) const noexcept;

    /// Convert to string
    std::string to_string() const;

  private:
    std::vector<identifier> layer_path_;
  };

  inline std::string format_as(layer_path const& lp) { return lp.to_string(); }
}

#endif // PHLEX_MODEL_LAYER_PATH_HPP
