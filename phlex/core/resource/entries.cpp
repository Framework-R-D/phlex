#include "phlex/core/resource/entries.hpp"

#include <boost/core/demangle.hpp>
#include <fmt/format.h>

#include <stdexcept>
#include <typeinfo>

namespace phlex::detail {
  [[noreturn]] void throw_empty_pooled_resource_error(std::type_info const& type)
  {
    throw std::runtime_error(fmt::format("Resource of type '{}' has an empty token pool",
                                         boost::core::demangle(type.name())));
  }
}
