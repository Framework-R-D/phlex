#include "phlex/model/algorithm_name.hpp"

#include <cassert>
#include <regex>
#include <tuple>

namespace {
  std::regex const algorithm_name_re{R"((\w+)?(:)?(\w+)?)"};
}

namespace phlex::experimental {
  algorithm_name::algorithm_name() = default;

  algorithm_name::algorithm_name(char const* name) : algorithm_name{std::string{name}} {}
  algorithm_name::algorithm_name(std::string name) { *this = create(name); }

  algorithm_name::algorithm_name(std::string plugin,
                                 std::string algorithm,
                                 specified_fields fields) :
    plugin_{std::move(plugin)}, algorithm_{std::move(algorithm)}, fields_{fields}
  {
  }

  std::string algorithm_name::full() const
  {
    std::string result{plugin_};
    if (not plugin_.empty()) {
      result += ":";
    }
    result += algorithm_;
    return result;
  }

  bool algorithm_name::match(algorithm_name const& other) const
  {
    switch (other.fields_) {
    case specified_fields::neither: {
      // Always return true if neither the plugin or algorithm is specified
      return true;
    }
    case specified_fields::either: {
      // Either the plugin or the algorithm can match
      if (other.plugin_.empty()) {
        return plugin_ == other.algorithm_ or algorithm_ == other.algorithm_;
      }
      assert(other.algorithm_.empty());
      return other.plugin_ == plugin_ or other.plugin_ == algorithm_;
    }
    case specified_fields::both: {
      // Exact equality expected if both the plugin or algorithm is specified
      return operator==(other);
    }
    }

    return false;
  }

  algorithm_name algorithm_name::create(char const* spec) { return create(std::string{spec}); }
  algorithm_name algorithm_name::create(std::string const& spec)
  {
    if (std::smatch matches; std::regex_match(spec, matches, algorithm_name_re)) {
      assert(matches.size() == 4ull);
      // If a colon ":" is specified, then both the plugin and algorithm must be specified.
      if (matches[2] == ":") {
        if (matches[3].str().empty()) {
          throw std::runtime_error("Cannot create an algorithm name that ends with a colon (':')");
        }
        return {matches[1], matches[3], specified_fields::both};
      }

      // Nothing specified
      if (matches[1].str().empty() and matches[3].str().empty()) {
        return {};
      }

      // Only one word is specified--could be either the plugin or the algorithm
      return {matches[1], matches[3], specified_fields::either};
    }
    throw std::runtime_error("The specification '" + spec + "' is not a valid algorithm name.");
  }
}
