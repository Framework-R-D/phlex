//form::detail::error() centralizes how FORM components report conditions that would usually be exceptions.  form_error() will throw something derived from std::runtime_error.

#include <source_location>
#include <string>

namespace form::detail::experimental
{
  void error(std::string const& message, std::source_location const where = std::source_location::current());
}
