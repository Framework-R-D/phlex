#ifndef PHLEX_CORE_INPUT_ARGUMENTS_HPP
#define PHLEX_CORE_INPUT_ARGUMENTS_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/model/handle.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace phlex::experimental {
  template <typename T>
  struct retriever {
    using handle_arg_t = detail::handle_value_type<T>;
    product_query query;
    auto retrieve(message const& msg) const
    {
      namespace views = std::ranges::views;
      auto const& store = msg.store;
      // TODO: This needs to be replaced with a properly engineered solution
      auto all_products = std::ranges::subrange(store->begin(), store->end()) | views::keys;
      auto products =
        all_products |
        views::filter([this](product_specification const& spec) { return query.match(spec); }) |
        views::transform([](product_specification const& spec) { return std::cref(spec); }) |
        std::ranges::to<std::vector>();
      if (products.empty()) {
        throw std::runtime_error(fmt::format(
          "No products found matching the query {}\n Store (id {} from {}) contains:\n    - {}",
          query,
          store->index()->to_string(),
          store->source().full(),
          fmt::join(all_products | views::transform(&product_specification::full), "\n    - ")));
      }
      if (products.size() > 1) {
        throw std::runtime_error(fmt::format(
          "Multiple products found matching the query {}:\n    - {}",
          query,
          fmt::join(products | views::transform(&product_specification::full), "\n    - ")));
      }
      return store->get_handle<handle_arg_t>(products[0]);
    }
  };

  template <typename InputTypes, std::size_t... Is>
  auto form_input_arguments_impl(product_queries const& args, std::index_sequence<Is...>)
  {
    return std::make_tuple(retriever<std::tuple_element_t<Is, InputTypes>>{args[Is]}...);
  }

  namespace detail {
    void verify_no_duplicate_input_products(std::string const& algorithm_name,
                                            product_queries to_sort);
  }

  template <typename InputTypes>
  auto form_input_arguments(std::string const& algorithm_name, product_queries const& args)
  {
    constexpr auto num_inputs = std::tuple_size_v<InputTypes>;
    detail::verify_no_duplicate_input_products(algorithm_name, args);
    return form_input_arguments_impl<InputTypes>(args, std::make_index_sequence<num_inputs>{});
  }

  template <typename InputTypes>
  using input_retriever_types = decltype(form_input_arguments<InputTypes>({}, {}));
}

#endif // PHLEX_CORE_INPUT_ARGUMENTS_HPP
