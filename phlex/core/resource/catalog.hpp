#ifndef PHLEX_CORE_RESOURCE_CATALOG_HPP
#define PHLEX_CORE_RESOURCE_CATALOG_HPP

#include "phlex/core/resource/entries.hpp"

#include "boost/core/demangle.hpp"

#include "fmt/format.h"

#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace phlex::detail {
  class resource_catalog {
  public:
    template <typename T, typename... Args>
      requires(!std::is_const_v<T> && phase_1_resource<T> && std::constructible_from<T, Args...>)
    void add(Args&&... args)
    {
      auto const type = std::type_index(typeid(T));
      if (resources_.contains(type)) {
        throw std::runtime_error(fmt::format("Resource of type '{}' has already been registered",
                                             boost::core::demangle(typeid(T).name())));
      }
      if constexpr (unlimited_resource<T>) {
        resources_.emplace(
          type, std::make_unique<unlimited_resource_entry<T>>(std::forward<Args>(args)...));
      } else {
        resources_.emplace(
          type, std::make_unique<single_token_resource_entry<T>>(std::forward<Args>(args)...));
      }
    }

    template <unlimited_resource T>
    gsl::not_null<T const*> access_for() const
    {
      auto* entry = entry_for<unlimited_resource_entry<T>>();
      assert(entry != nullptr && "resource catalog entry has unexpected type");
      return entry->access();
    }

    template <single_token_resource T>
    auto& limiter_for() const
    {
      auto* entry = entry_for<single_token_resource_entry<T>>();
      assert(entry != nullptr && "resource catalog entry has unexpected type");
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

    std::map<std::type_index, std::unique_ptr<resource_base>> resources_;
  };
}

#endif // PHLEX_CORE_RESOURCE_CATALOG_HPP
