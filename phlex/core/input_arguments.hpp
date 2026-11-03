#ifndef PHLEX_CORE_INPUT_ARGUMENTS_HPP
#define PHLEX_CORE_INPUT_ARGUMENTS_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/phlex_core_export.hpp"

#include <gsl/pointers>

#include <cstddef>
#include <tuple>
#include <utility>

namespace phlex::detail {
  namespace internal {
    PHLEX_CORE_EXPORT gsl::not_null<phlex::experimental::product_specification const*>
    resolve_product(product_selector const& query, phlex::experimental::product_store const& store);
  }

  template <typename T>
  struct retriever {
    using handle_arg_t = internal::handle_value_type<T>;
    product_selector query;
    auto retrieve(message const& msg) const
    {
      auto const& store = msg.store;
      return store->get_handle<handle_arg_t>(internal::resolve_product(query, *store));
    }
  };

  template <typename InputTypes, std::size_t... Is>
  auto form_input_arguments_impl(product_selectors const& args, std::index_sequence<Is...>)
  {
    return std::make_tuple(retriever<std::tuple_element_t<Is, InputTypes>>{args[Is]}...);
  }

  template <typename InputTypes>
  auto form_input_arguments(product_selectors const& args)
  {
    constexpr auto num_inputs = std::tuple_size_v<InputTypes>;
    return form_input_arguments_impl<InputTypes>(args, std::make_index_sequence<num_inputs>{});
  }

  template <typename InputTypes>
  using input_retriever_types = decltype(form_input_arguments<InputTypes>({}));
}

#endif // PHLEX_CORE_INPUT_ARGUMENTS_HPP
