#include "error.hpp"

#include <source_location>
#include <stdexcept>

namespace form::detail::experimental
{
  void error(std::string const& message, std::source_location const where)
  {
    throw std::runtime_error(std::string(where.function_name()) + ": " + message);
  }
}
