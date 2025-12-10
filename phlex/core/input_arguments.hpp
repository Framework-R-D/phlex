#ifndef PHLEX_CORE_INPUT_ARGUMENTS_HPP
#define PHLEX_CORE_INPUT_ARGUMENTS_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/model/handle.hpp"

#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace phlex::experimental {
  template <typename T, std::size_t JoinNodePort>
  struct retriever {
    using handle_arg_t = detail::handle_value_type<T>;
    product_query query;
    auto retrieve(auto const& messages) const
    {
      return std::get<JoinNodePort>(messages).store->template get_handle<handle_arg_t>(
        query.spec.name());
    }
  };

  template <typename InputTypes>
  struct input_retriever_types_impl {
    template <std::size_t... Is>
    static std::tuple<retriever<std::tuple_element_t<Is, InputTypes>, Is>...> form_tuple(
      std::index_sequence<Is...>);

    using type = decltype(form_tuple(std::make_index_sequence<std::tuple_size_v<InputTypes>>{}));
  };

  template <typename InputTypes>
  using input_retriever_types = typename input_retriever_types_impl<InputTypes>::type;

  template <typename InputTypes, std::size_t... Is>
  auto form_input_arguments_impl(product_queries const& args, std::index_sequence<Is...>)
  {
    return std::make_tuple(
      retriever<std::tuple_element_t<Is, InputTypes>, Is>{std::move(args[Is])}...);
  }

  namespace detail {
    void verify_no_duplicate_input_products(std::string const& algorithm_name,
                                            product_queries to_sort);
  }

  template <typename InputTypes>
  auto form_input_arguments(std::string const& algorithm_name, product_queries const& args)
  {
    constexpr auto N = std::tuple_size_v<InputTypes>;
    detail::verify_no_duplicate_input_products(algorithm_name, args);
    return form_input_arguments_impl<InputTypes>(args, std::make_index_sequence<N>{});
  }
}

#endif // PHLEX_CORE_INPUT_ARGUMENTS_HPP
