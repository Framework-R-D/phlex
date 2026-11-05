#ifndef PHLEX_MODEL_ALGORITHM_NAME_HPP
#define PHLEX_MODEL_ALGORITHM_NAME_HPP

#include "phlex/model/identifier.hpp"
#include "phlex/phlex_model_export.hpp"

#include <cstdint>

namespace phlex::experimental {
  /// The creator of a product or the name of an algorithm.
  ///
  /// An algorithm name always has a non-empty algorithm component. Its plugin component is either
  /// a non-empty plugin name or explicitly empty (for an unqualified name). A default-constructed
  /// algorithm name is not valid, so one must always be constructed from a name.
  class PHLEX_MODEL_EXPORT algorithm_name {
  public:
    algorithm_name() = delete;

    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    algorithm_name(char const* spec);
    algorithm_name(std::string const& spec);
    algorithm_name(std::string_view spec);
    // NOLINTEND(google-explicit-constructor)
    algorithm_name(identifier_or_empty plugin, identifier algorithm);

    std::string to_string() const;
    identifier_or_empty const& plugin() const noexcept { return plugin_; }
    identifier const& algorithm() const noexcept { return algorithm_; }

    bool match(algorithm_name const& other) const;
    auto operator<=>(algorithm_name const&) const noexcept = default;

    /// Parses a creator name.
    ///
    /// Accepted forms:
    /// - `algorithm`: the plugin component is explicitly empty.
    /// - `plugin:algorithm`: both components are non-empty names.
    ///
    /// Each component must consist of word characters only. Rejected inputs include the empty
    /// string, a leading colon (`:algorithm`), a trailing colon (`plugin:`), and any empty
    /// component. Throws `std::runtime_error` for every rejected input.
    static algorithm_name create(char const* spec);
    static algorithm_name create(std::string_view spec);

  private:
    identifier_or_empty plugin_;
    identifier algorithm_;
  };

}

#endif // PHLEX_MODEL_ALGORITHM_NAME_HPP
