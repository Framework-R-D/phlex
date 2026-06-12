#include "phlex/model/algorithm_name.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <cassert>

namespace {
  // Check if a char is a word (\w) char
  // Only care about C locale (ASCII) for now
  bool is_word(char c)
  {
    if ('0' <= c && c <= '9') {
      return true;
    }
    if ('A' <= c && c <= 'Z') {
      return true;
    }
    if ('a' <= c && c <= 'z') {
      return true;
    }
    if (c == '_' || c == '[' || c == ']') {
      return true;
    }
    return false;
  }

  // Check that a string_view only contains word chars
  bool words_only(std::string_view str) { return std::ranges::all_of(str, is_word); }
}

namespace phlex::experimental {
  algorithm_name::algorithm_name() = default;

  algorithm_name::algorithm_name(char const* spec) : algorithm_name{std::string_view{spec}} {}
  algorithm_name::algorithm_name(std::string const& spec) : algorithm_name{std::string_view{spec}}
  {
  }
  algorithm_name::algorithm_name(std::string_view spec) { *this = create(spec); }

  algorithm_name::algorithm_name(identifier plugin, identifier algorithm) :
    plugin_{std::move(plugin)}, algorithm_{std::move(algorithm)}
  {
  }

  std::string algorithm_name::to_string() const
  {
    if (!plugin_.empty()) {
      return fmt::format("{}:{}", plugin_, algorithm_);
    }
    // This will stay after trans_get_string is removed
    return fmt::format("{}", algorithm_);
  }

  bool algorithm_name::match(algorithm_name const& other) const { return operator==(other); }

  algorithm_name algorithm_name::create(char const* spec) { return create(std::string_view{spec}); }
  algorithm_name algorithm_name::create(std::string_view spec)
  {
    // Empty spec means nothing is specified
    if (spec.empty()) {
      return {};
    }

    // Do we have a colon? If so, have both plugin and algorithm
    if (spec.contains(':')) {
      if (spec.ends_with(':')) {
        throw std::runtime_error("Cannot create an algorithm name that ends with a colon (':')");
      }
      auto const colon_pos = spec.find(':');
      std::string_view const plugin = spec.substr(0, colon_pos);
      std::string_view const algorithm = spec.substr(colon_pos + 1); // +1 OK, : not at end

      if (!words_only(plugin) or !words_only(algorithm)) {
        throw std::runtime_error(
          fmt::format("The specification '{}' is not a valid algorithm name.", spec));
      }
      return {identifier(plugin), identifier(algorithm)};
    }

    // No colon -- could be plugin or algorithm
    if (!words_only(spec)) {
      throw std::runtime_error(
        fmt::format("The specification '{}' is not a valid algorithm name.", spec));
    }
    return {"", identifier(spec)};
  }
}
