// Copyright (C) 2025 ...

#ifndef FORM_FORM_FORM_SOURCE_TYPE_REGISTRY_HPP
#define FORM_FORM_FORM_SOURCE_TYPE_REGISTRY_HPP

#include "phlex/model/products.hpp"
#include "util/error.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace form::experimental {

  using form_source_product_from_data_fn = std::function<phlex::experimental::product_ptr(
    void const* data, std::string const& product_name, std::string const& index_str)>;

  struct form_source_type_entry {
    std::type_info const* cpp_type{nullptr};
    form_source_product_from_data_fn product_from_data_fn;
  };

  void register_form_product_type(std::type_info const& cpp_type,
                                  form_source_product_from_data_fn product_from_data_fn);

  form_source_type_entry const* find_form_product_type(std::type_index product_type);
  void ensure_builtin_form_product_types_registered();

  template <typename T>
  void register_form_product_cpp_type()
  {
    using product_type_t = std::remove_cvref_t<T>;

    auto product_from_data_fn =
      [](void const* data,
         std::string const& product_name,
         std::string const& index_str) -> phlex::experimental::product_ptr {
      if (!data) {
        form::detail::experimental::error("Failed to retrieve product [" + product_name + "] for " +
                                          index_str);
      }

      auto ptr = std::unique_ptr<product_type_t const>(static_cast<product_type_t const*>(data));
      return phlex::experimental::product_for(*ptr);
    };

    register_form_product_type(typeid(product_type_t), std::move(product_from_data_fn));
  }

  template <typename T>
  void register_form_vector_product_type()
  {
    register_form_product_cpp_type<std::vector<T>>();
  }

} // namespace form::experimental

#endif // FORM_FORM_FORM_SOURCE_TYPE_REGISTRY_HPP
