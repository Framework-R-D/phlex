// Copyright (C) 2025 ...

#include "form_source_type_registry.hpp"

#include <mutex>
#include <unordered_map>

namespace {
  std::unordered_map<std::string, form::experimental::form_source_type_entry>&
  mutable_form_type_registry()
  {
    static std::unordered_map<std::string, form::experimental::form_source_type_entry> registry;
    return registry;
  }

  std::mutex& form_type_registry_mutex()
  {
    static std::mutex registry_mutex;
    return registry_mutex;
  }
}

namespace form::experimental {

  void register_form_product_type(std::string product_type,
                                  phlex::experimental::type_id type,
                                  std::type_info const& cpp_type,
                                  form_source_reader_fn reader_fn)
  {
    if (product_type.empty()) {
      throw std::runtime_error("Cannot register empty FORM product type name");
    }
    if (!reader_fn) {
      throw std::runtime_error("Cannot register FORM product type with empty reader function");
    }

    std::lock_guard<std::mutex> lock(form_type_registry_mutex());
    mutable_form_type_registry()[std::move(product_type)] =
      form_source_type_entry{std::move(type), &cpp_type, std::move(reader_fn)};
  }

  form_source_type_entry const* find_form_product_type(std::string const& product_type)
  {
    ensure_builtin_form_product_types_registered();

    std::lock_guard<std::mutex> lock(form_type_registry_mutex());
    auto const& registry = mutable_form_type_registry();
    auto const it = registry.find(product_type);
    if (it == registry.end()) {
      return nullptr;
    }
    return &it->second;
  }

  std::string const* find_form_product_type_name(phlex::experimental::type_id const& type)
  {
    ensure_builtin_form_product_types_registered();

    std::lock_guard<std::mutex> lock(form_type_registry_mutex());
    auto const& registry = mutable_form_type_registry();
    for (auto const& [name, entry] : registry) {
      if (entry.type_id == type) {
        return &name;
      }
    }
    return nullptr;
  }

  void ensure_builtin_form_product_types_registered()
  {
    static std::once_flag once;
    std::call_once(once, [] {
      register_form_vector_product_type<int>("std::vector<int>");
      register_form_vector_product_type<unsigned int>("std::vector<unsigned int>");
      register_form_vector_product_type<long>("std::vector<long>");
      register_form_vector_product_type<unsigned long>("std::vector<unsigned long>");
      register_form_vector_product_type<long long>("std::vector<long long>");
      register_form_vector_product_type<unsigned long long>("std::vector<unsigned long long>");
      register_form_vector_product_type<float>("std::vector<float>");
      register_form_vector_product_type<double>("std::vector<double>");
      register_form_vector_product_type<bool>("std::vector<bool>");
      register_form_vector_product_type<char>("std::vector<char>");
      register_form_vector_product_type<std::string>("std::vector<std::string>");
    });
  }

} // namespace form::experimental
