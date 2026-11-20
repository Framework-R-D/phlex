#include "phlex/model/algorithm_name.hpp"

#include "phlex/model/detail/excerpt.hpp"
#include "phlex/model/identifier.hpp"

#include <fmt/format.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
  constexpr std::string_view expected_forms{R"(Expected "algorithm" or "plugin_label:algorithm".)"};

  [[noreturn]] void throw_invalid(std::string_view name, std::string_view reason)
  {
    throw std::invalid_argument{
      fmt::format("Invalid algorithm name {}: {}", phlex::detail::excerpt(name), reason)};
  }

  // Constructing an identifier is the one place where character rules are checked. The
  // identifier's own message is replaced with one that says which part of the name is at fault.
  phlex::experimental::identifier make_identifier(std::string_view name,
                                                  std::string_view part,
                                                  std::string_view text)
  {
    try {
      return phlex::experimental::identifier{text};
    } catch (std::invalid_argument const&) {
      throw_invalid(
        name,
        fmt::format(
          "{} {} {}", part, phlex::detail::excerpt(text), phlex::detail::identifier_rule));
    }
  }
}

namespace phlex::experimental {
  namespace {
    // Splits `spec` into an optional plugin label and an algorithm, then validates them through
    // the component overload of `algorithm_name::create`.
    algorithm_name parse(std::string_view spec)
    {
      auto const colon = spec.find(':');
      if (colon == std::string_view::npos) {
        return algorithm_name::create(std::nullopt, spec);
      }
      auto const algorithm = spec.substr(colon + 1);
      if (algorithm.contains(':')) {
        throw_invalid(spec, fmt::format("it contains more than one ':'. {}", expected_forms));
      }
      return algorithm_name::create(spec.substr(0, colon), algorithm);
    }
  }

  algorithm_name::algorithm_name(char const* spec) : algorithm_name{std::string_view{spec}} {}
  algorithm_name::algorithm_name(std::string const& spec) : algorithm_name{std::string_view{spec}}
  {
  }
  algorithm_name::algorithm_name(std::string_view spec) : algorithm_name{parse(spec)} {}

  algorithm_name::algorithm_name(std::optional<identifier> plugin, identifier algorithm) :
    plugin_{std::move(plugin)}, algorithm_{std::move(algorithm)}
  {
  }

  std::string algorithm_name::to_string() const
  {
    if (plugin_) {
      return fmt::format("{}:{}", *plugin_, algorithm_);
    }
    return fmt::format("{}", algorithm_);
  }

  bool algorithm_name::match(algorithm_name const& other) const { return operator==(other); }

  algorithm_name algorithm_name::create(std::optional<std::string_view> plugin,
                                        std::string_view algorithm)
  {
    auto const name = plugin ? fmt::format("{}:{}", *plugin, algorithm) : std::string{algorithm};

    if (!plugin && algorithm.empty()) {
      throw_invalid(name, fmt::format("it is empty. {}", expected_forms));
    }
    if (plugin && plugin->empty()) {
      throw_invalid(name,
                    fmt::format("the plugin label before ':' is missing. {}", expected_forms));
    }
    if (plugin && algorithm.empty()) {
      throw_invalid(name, fmt::format("the algorithm after ':' is missing. {}", expected_forms));
    }

    std::optional<identifier> plugin_label;
    if (plugin) {
      plugin_label.emplace(make_identifier(name, "plugin label", *plugin));
    }
    return {std::move(plugin_label), make_identifier(name, "algorithm", algorithm)};
  }
}
