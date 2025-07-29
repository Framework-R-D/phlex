#include "phlex/utilities/stripped_name.hpp"

#include <regex>

namespace {
  std::regex const keep_up_to_paren_right_before_function_params{R"((.*\w)\(.*)"};
}

namespace phlex::experimental::detail {

  std::string stripped_name(std::string full_name)
  {
    full_name = std::regex_replace(full_name, keep_up_to_paren_right_before_function_params, "$1");
    // Remove any upfront qualifiers
    return full_name.substr(full_name.find_last_of(":") + 1ull);
  }
}
