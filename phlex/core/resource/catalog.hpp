#ifndef PHLEX_CORE_RESOURCE_CATALOG_HPP
#define PHLEX_CORE_RESOURCE_CATALOG_HPP

#include "phlex/core/resource/entries.hpp"
#include "phlex/phlex_core_export.hpp"

#include <gsl/pointers>

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace phlex::detail {
  class PHLEX_CORE_EXPORT resource_catalog {
  public:
    template <typename T, typename... Args>
      requires unlimited_resource_registration<T, Args...>
    void add_unlimited(Args&&... args)
    {
      auto const type = std::type_index(typeid(T));
      check_available(type);
      insert(type, std::make_unique<unlimited_resource_entry<T>>(std::forward<Args>(args)...));
    }

    template <typename T, typename... Args>
      requires serialized_resource_registration<T, Args...>
    void add_serialized(Args&&... args)
    {
      auto const type = std::type_index(typeid(T));
      check_available(type);
      insert(type, std::make_unique<serialized_resource_entry<T>>(std::forward<Args>(args)...));
    }

    template <unlimited_resource T>
    gsl::not_null<T const*> access_for() const
    {
      auto entry = entry_for<unlimited_resource_entry<T>>();
      return entry->access();
    }

    template <serialized_resource T>
    auto& limiter_for() const
    {
      auto entry = entry_for<serialized_resource_entry<T>>();
      return entry->limiter();
    }

  private:
    void check_available(std::type_index type) const;
    void insert(std::type_index type, std::unique_ptr<resource_base> entry);
    gsl::not_null<resource_base*> entry_for(std::type_index type) const;

    template <typename Entry>
    gsl::not_null<Entry*> entry_for() const
    {
      using resource_type = Entry::resource_type;
      auto const entry = entry_for(std::type_index(typeid(resource_type)));
      // The dynamic cast must succeed based on the construction of the catalog; the
      // 'gsl::not_null' constructor expects this as a precondition and will terminate if the
      // precondition is not satisfied.
      return gsl::not_null{dynamic_cast<Entry*>(entry.get())};
    }

    std::unordered_map<std::type_index, std::unique_ptr<resource_base>> resources_;
  };
}

#endif // PHLEX_CORE_RESOURCE_CATALOG_HPP
