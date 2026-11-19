//form::experimental::detail::error() centralizes how FORM components report conditions that would usually be exceptions.  error() will throw something derived from std::runtime_error.

#ifndef FORM_UTIL_ERROR_HPP
#define FORM_UTIL_ERROR_HPP

#include <source_location>
#include <string>

namespace form::detail::experimental {
  [[noreturn]] void error(std::string const& message,
                          std::source_location where = std::source_location::current());
}

#endif //FORM_UTIL_ERROR_HPP
