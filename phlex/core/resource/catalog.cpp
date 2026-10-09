#include "phlex/core/resource/catalog.hpp"

#include "phlex/core/resource/entries.hpp"

#include <boost/core/demangle.hpp>
#include <fmt/format.h>
#include <gsl/pointers>

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace phlex::detail {
  void resource_catalog::check_available(std::type_index type) const
  {
    if (resources_.contains(type)) {
      throw std::runtime_error(fmt::format("Resource of type '{}' has already been registered",
                                           boost::core::demangle(type.name())));
    }
  }

  void resource_catalog::insert(std::type_index type, std::unique_ptr<resource_base> entry)
  {
    resources_.emplace(type, std::move(entry));
  }

  gsl::not_null<resource_base*> resource_catalog::entry_for(std::type_index type) const
  {
    auto const found = resources_.find(type);
    if (found == resources_.end()) {
      throw std::runtime_error(fmt::format("Resource of type '{}' has not been registered",
                                           boost::core::demangle(type.name())));
    }
    return gsl::not_null{found->second.get()};
  }
}
