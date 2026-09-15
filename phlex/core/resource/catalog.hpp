#ifndef PHLEX_CORE_RESOURCE_CATALOG_HPP
#define PHLEX_CORE_RESOURCE_CATALOG_HPP

#include "phlex/core/resource/entries.hpp"

#include "boost/core/demangle.hpp"

#include "fmt/format.h"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace phlex::detail {
  class resource_catalog {
  public:
    template <typename T, typename... Args>
      requires unlimited_resource_registration<T, Args...>
    void add_unlimited(Args&&... args)
    {
      auto const type = std::type_index(typeid(T));
      if (resources_.contains(type)) {
        throw std::runtime_error(fmt::format("Resource of type '{}' has already been registered",
                                             boost::core::demangle(typeid(T).name())));
      }
      resources_.emplace(
        type, std::make_unique<unlimited_resource_entry<T>>(std::forward<Args>(args)...));
    }

    template <typename T, typename... Args>
      requires serialized_resource_registration<T, Args...>
    void add_serialized(Args&&... args)
    {
      auto const type = std::type_index(typeid(T));
      if (resources_.contains(type)) {
        throw std::runtime_error(fmt::format("Resource of type '{}' has already been registered",
                                             boost::core::demangle(typeid(T).name())));
      }
      resources_.emplace(
        type, std::make_unique<serialized_resource_entry<T>>(std::forward<Args>(args)...));
    }

    template <unlimited_resource T>
    gsl::not_null<T const*> access_for() const
    {
      auto* entry = entry_for<unlimited_resource_entry<T>>();
      assert(entry != nullptr && "resource catalog entry has unexpected unlimited entry type");
      return entry->access();
    }

    template <serialized_resource T>
    auto& limiter_for() const
    {
      auto* entry = entry_for<serialized_resource_entry<T>>();
      assert(entry != nullptr && "resource catalog entry has unexpected serialized entry type");
      return entry->limiter();
    }

  private:
    template <typename Entry>
    Entry* entry_for() const
    {
      using resource_type = Entry::resource_type;
      auto const found = resources_.find(std::type_index(typeid(resource_type)));
      if (found == resources_.end()) {
        throw std::runtime_error(fmt::format("Resource of type '{}' has not been registered",
                                             boost::core::demangle(typeid(resource_type).name())));
      }
      return dynamic_cast<Entry*>(found->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<resource_base>> resources_;
  };
}

#endif // PHLEX_CORE_RESOURCE_CATALOG_HPP
