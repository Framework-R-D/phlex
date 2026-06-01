#ifndef PHLEX_MODEL_ALGORITHM_NAME_HPP
#define PHLEX_MODEL_ALGORITHM_NAME_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/identifier.hpp"

#include <cstdint>

namespace phlex::experimental {
  class PHLEX_MODEL_EXPORT algorithm_name {
  public:
    algorithm_name();

    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    algorithm_name(char const* spec);
    algorithm_name(std::string const& spec);
    algorithm_name(std::string_view spec);
    // NOLINTEND(google-explicit-constructor)
    algorithm_name(identifier plugin, identifier algorithm);

    std::string to_string() const;
    identifier const& plugin() const noexcept { return plugin_; }
    identifier const& algorithm() const noexcept { return algorithm_; }

    bool match(algorithm_name const& other) const;
    auto operator<=>(algorithm_name const&) const noexcept = default;

    static algorithm_name create(char const* spec);
    static algorithm_name create(std::string_view spec);

  private:
    identifier plugin_;
    identifier algorithm_;
  };

}

#endif // PHLEX_MODEL_ALGORITHM_NAME_HPP
