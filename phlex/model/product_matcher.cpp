#include "phlex/model/product_matcher.hpp"


#include <array>
#include <cassert>
#include <fmt/core.h>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
  auto value_or(std::string const& value, std::string const& default_value)
  {
    return value.empty() ? default_value : value;
  }

  auto tokenize(std::string matcher_spec)
  {
    if (empty(matcher_spec)) {
      throw std::runtime_error("Empty product-matcher specifications are not allowed.");
    }
    if (matcher_spec[0] == '/') {
      throw std::runtime_error("The matcher specification may not start with a forward slash (/).");
    }

    std::string const optional_level_path{R"((?:(.*)/)?)"};
    std::string const optional_qualified_node_name{R"((?:(\w+)?(?:@(\w+))?:)?)"};
    std::string const product_name{R"((\w+))"};
    std::regex const pattern{optional_level_path + optional_qualified_node_name + product_name};

    std::smatch submatches;
    bool const matched = std::regex_match(matcher_spec, submatches, pattern);
    if (not matched) {
      throw std::runtime_error(
        "Provided product specification does not match the supported pattern:\n"
        "\"[level path spec/][[module name][@node name]:]product name\"");
    }
    assert(submatches.size() == 5);
    assert(submatches[0] == matcher_spec);
    return std::array<std::string, 4u>{
      value_or(submatches[1], "*"), submatches[2], submatches[3], submatches[4]};
  }
}

namespace phlex::experimental {
  product_matcher::product_matcher(std::string matcher_spec) :
    product_matcher{tokenize(std::move(matcher_spec))}
  {
  }

  product_matcher::product_matcher(std::array<std::string, 4u> fields) :
    level_path_{std::move(fields[0])},
    module_name_{std::move(fields[1])},
    node_name_{std::move(fields[2])},
    product_name_{std::move(fields[3])}
  {
  }

  auto product_matcher::encode() const -> std::string
  {
    return fmt::format("{}/{}@{}:{}", level_path_, module_name_, node_name_, product_name_);
  }
}
