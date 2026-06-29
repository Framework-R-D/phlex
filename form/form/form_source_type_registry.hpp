// Copyright (C) 2025 ...

#ifndef FORM_FORM_FORM_SOURCE_TYPE_REGISTRY_HPP
#define FORM_FORM_FORM_SOURCE_TYPE_REGISTRY_HPP

#include "form_reader.hpp"

#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace form::experimental {

  using form_source_reader_fn =
    std::function<phlex::experimental::product_ptr(form_reader_interface& reader,
                                                   std::string const& creator,
                                                   std::string const& product_name,
                                                   std::string const& index_str,
                                                   std::string const& product_type)>;

  struct form_source_type_entry {
    phlex::experimental::type_id type_id;
    std::type_info const* cpp_type{nullptr};
    form_source_reader_fn reader_fn{};
  };

  void register_form_product_type(std::string product_type,
                                  phlex::experimental::type_id type,
                                  std::type_info const& cpp_type,
                                  form_source_reader_fn reader_fn);

  form_source_type_entry const* find_form_product_type(std::string const& product_type);
  std::string const* find_form_product_type_name(phlex::experimental::type_id const& type);
  void ensure_builtin_form_product_types_registered();

  template <typename T>
  void register_form_product_cpp_type(std::string product_type)
  {
    using product_type_t = std::remove_cvref_t<T>;

    auto reader_fn =
      [](form_reader_interface& reader,
         std::string const& creator,
         std::string const& product_name,
         std::string const& index_str,
         std::string const& runtime_product_type) -> phlex::experimental::product_ptr {
      (void)runtime_product_type;
      product_with_name pb{product_name, nullptr, &typeid(product_type_t)};
      reader.read(creator, index_str, pb);
      if (!pb.data) {
        throw std::runtime_error("FORM Error: Failed to retrieve product [" + product_name +
                                 "] for " + index_str);
      }
      return phlex::experimental::product_for(*static_cast<product_type_t const*>(pb.data));
    };

    register_form_product_type(std::move(product_type),
                               phlex::experimental::make_type_id<product_type_t>(),
                               typeid(product_type_t),
                               std::move(reader_fn));
  }

  template <typename T>
  void register_form_vector_product_type(std::string product_type)
  {
    register_form_product_cpp_type<std::vector<T>>(std::move(product_type));
  }

} // namespace form::experimental

#endif // FORM_FORM_FORM_SOURCE_TYPE_REGISTRY_HPP
