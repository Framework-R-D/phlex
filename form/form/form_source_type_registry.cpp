// Copyright (C) 2025 ...

#include "form_source_type_registry.hpp"

#include <mutex>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace {
  std::unordered_map<std::type_index, form::experimental::form_source_type_entry>&
  mutable_form_type_registry()
  {
    static std::unordered_map<std::type_index, form::experimental::form_source_type_entry> registry;
    return registry;
  }

  std::mutex& form_type_registry_mutex()
  {
    static std::mutex registry_mutex;
    return registry_mutex;
  }
}

namespace form::experimental {

  void register_form_product_type(std::type_info const& cpp_type,
                                  form_source_product_from_data_fn product_from_data_fn)
  {
    if (!product_from_data_fn) {
      form::detail::experimental::error(
        "Cannot register FORM product type with empty conversion function");
    }

    std::scoped_lock const lock(form_type_registry_mutex());
    mutable_form_type_registry()[std::type_index{cpp_type}] = form_source_type_entry{
      .cpp_type = &cpp_type, .product_from_data_fn = std::move(product_from_data_fn)};
  }

  // Returns a pointer to the registry entry.
  // Caller must not hold the returned pointer across calls that might modify the registry.
  form_source_type_entry const* find_form_product_type(std::type_index product_type)
  {
    ensure_builtin_form_product_types_registered();

    std::scoped_lock const lock(form_type_registry_mutex());
    auto const& registry = mutable_form_type_registry();
    auto const it = registry.find(product_type);
    if (it == registry.end()) {
      return nullptr;
    }
    return &it->second;
  }

  void ensure_builtin_form_product_types_registered()
  {
    static std::once_flag once;
    std::call_once(once, [] {
      register_form_vector_product_type<int>();
      register_form_vector_product_type<unsigned int>();
      register_form_vector_product_type<long>();
      register_form_vector_product_type<unsigned long>();
      register_form_vector_product_type<long long>();
      register_form_vector_product_type<unsigned long long>();
      register_form_vector_product_type<float>();
      register_form_vector_product_type<double>();
      register_form_vector_product_type<bool>();
      register_form_vector_product_type<char>();
      register_form_vector_product_type<std::string>();
    });
  }

} // namespace form::experimental
