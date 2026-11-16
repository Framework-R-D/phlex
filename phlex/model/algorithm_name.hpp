#ifndef PHLEX_MODEL_ALGORITHM_NAME_HPP
#define PHLEX_MODEL_ALGORITHM_NAME_HPP

#include "phlex/model/identifier.hpp"
#include "phlex/phlex_model_export.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace phlex::experimental {
  class PHLEX_MODEL_EXPORT algorithm_name {
  public:
    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    algorithm_name(char const* spec);
    algorithm_name(std::string const& spec);
    algorithm_name(std::string_view spec);
    // NOLINTEND(google-explicit-constructor)
    /// Constructs a name from an optional plugin qualifier and a required algorithm.
    /// An unqualified name is represented by `std::nullopt`.
    algorithm_name(std::optional<identifier> plugin, identifier algorithm);

    std::string to_string() const;
    std::optional<identifier> const& plugin() const noexcept { return plugin_; }
    identifier const& algorithm() const noexcept { return algorithm_; }

    bool match(algorithm_name const& other) const;
    auto operator<=>(algorithm_name const&) const noexcept = default;

    static algorithm_name create(char const* spec);
    static algorithm_name create(std::string_view spec);

  private:
    std::optional<identifier> plugin_;
    identifier algorithm_;
  };

}

#endif // PHLEX_MODEL_ALGORITHM_NAME_HPP
