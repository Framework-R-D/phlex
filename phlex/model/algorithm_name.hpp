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
    /// Parse text that is either `algorithm` or `plugin_label:algorithm`.
    ///
    /// Throws `std::invalid_argument` if `spec` is malformed. The message quotes the offending
    /// text and says which part of the name is invalid and why. Only the first problem found is
    /// reported.
    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    algorithm_name(char const* spec);
    algorithm_name(std::string const& spec);
    algorithm_name(std::string_view spec);
    // NOLINTEND(google-explicit-constructor)
    /// Constructs a name from an optional plugin label and a required algorithm.
    /// An unqualified name is represented by `std::nullopt`.
    algorithm_name(std::optional<identifier> plugin, identifier algorithm);

    std::string to_string() const;
    std::optional<identifier> const& plugin() const noexcept { return plugin_; }
    identifier const& algorithm() const noexcept { return algorithm_; }

    bool match(algorithm_name const& other) const;
    auto operator<=>(algorithm_name const&) const noexcept = default;

    /// Validates the components of a name and constructs it. This is the single place where
    /// the text of an algorithm name is checked: the character rules are those of `identifier`,
    /// and failures are reported as described for the text constructors, which parse their
    /// argument and then call this function.
    ///
    /// An unqualified name is represented by `std::nullopt`. An empty `plugin` is not the same
    /// as `std::nullopt`; it is rejected.
    static algorithm_name create(std::optional<std::string_view> plugin,
                                 std::string_view algorithm);

  private:
    std::optional<identifier> plugin_;
    identifier algorithm_;
  };

}

#endif // PHLEX_MODEL_ALGORITHM_NAME_HPP
