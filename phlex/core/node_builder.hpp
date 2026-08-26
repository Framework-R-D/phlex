#ifndef PHLEX_CORE_NODE_BUILDER_HPP
#define PHLEX_CORE_NODE_BUILDER_HPP

#include "phlex/core/resource_api.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <gsl/span>

#include <array>
#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>

namespace phlex::detail {

  using no_outputs_t = std::tuple<>;

  template <typename... Outputs>
  struct multifunction_outputs {};

  template <typename InputMessages, typename Function, typename Outputs, typename Resources>
  struct node_builder;

  // TBB node template arguments model product-message flow only. Resource access is supplied
  // separately by the node body: unlimited resources come from resource_catalog, while TBB
  // acquires bounded-resource tokens from the resource_limiter objects passed to the node. Each
  // output shape is split into two mutually exclusive specializations, selected by a requires
  // clause on has_bounded<Resources...>.
  template <typename InputMessages, typename Outputs, typename Resources>
  struct node_type_for;

  template <typename InputMessages, typename... Resources>
    requires(not has_bounded<Resources...>)
  struct node_type_for<InputMessages, no_outputs_t, std::tuple<Resources...>> {
    using type = tbb::flow::function_node<InputMessages>;
  };

  template <typename InputMessages, typename... Resources>
    requires(has_bounded<Resources...>)
  struct node_type_for<InputMessages, no_outputs_t, std::tuple<Resources...>> {
    using type = tbb::flow::resource_limited_node<InputMessages, no_outputs_t>;
  };

  template <typename InputMessages, typename Output, typename... Resources>
    requires(not has_bounded<Resources...>)
  struct node_type_for<InputMessages, std::tuple<Output>, std::tuple<Resources...>> {
    using type = tbb::flow::function_node<InputMessages, Output>;
  };

  template <typename InputMessages, typename Output, typename... Resources>
    requires(has_bounded<Resources...>)
  struct node_type_for<InputMessages, std::tuple<Output>, std::tuple<Resources...>> {
    using type = tbb::flow::resource_limited_node<InputMessages, std::tuple<Output>>;
  };

  template <typename InputMessages, typename... Outputs, typename... Resources>
    requires(not has_bounded<Resources...>)
  struct node_type_for<InputMessages, multifunction_outputs<Outputs...>, std::tuple<Resources...>> {
    using type = tbb::flow::multifunction_node<InputMessages, std::tuple<Outputs...>>;
  };

  template <typename InputMessages, typename... Outputs, typename... Resources>
    requires(has_bounded<Resources...>)
  struct node_type_for<InputMessages, multifunction_outputs<Outputs...>, std::tuple<Resources...>> {
    using type = tbb::flow::resource_limited_node<InputMessages, std::tuple<Outputs...>>;
  };

  template <typename InputMessages, typename Outputs, typename Resources>
  using node_type_for_t = node_type_for<InputMessages, Outputs, Resources>::type;

  // For each resource, in declaration order, the index it would occupy within just the
  // unlimited or just the bounded subsequence, so a single pass can pick its argument
  // from the right tuple.
  template <typename... Resources>
  inline constexpr auto resource_local_indices = [] {
    std::array<std::size_t, sizeof...(Resources)> indices{};
    std::size_t bounded = 0;
    std::size_t unlimited = 0;
    std::size_t i = 0;
    // gsl::at used to avoid cppcoreguidelines-pro-bounds-constant-array-index clang-tidy warning.
    ((gsl::at(indices, i++) = unlimited_resource<Resources> ? unlimited++ : bounded++), ...);
    return indices;
  }();

  template <typename Resource, std::size_t LocalIndex>
  decltype(auto) resource_argument(auto const& unlimited_resources, auto&& bounded_tokens)
  {
    if constexpr (unlimited_resource<Resource>) {
      return std::get<LocalIndex>(unlimited_resources);
    } else {
      return std::get<LocalIndex>(bounded_tokens);
    }
  }

  // Reassembles Resources... in declaration order for the node body, drawing each argument
  // from either the per-call bounded_tokens (TBB-supplied) or the cached unlimited_resources.
  template <typename... Resources>
  decltype(auto) invoke_with_resources(auto const& callable,
                                       auto&& arguments,
                                       auto&& bounded_tokens,
                                       auto const& unlimited_resources)
  {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
      return std::apply(
        [&]<typename... FunctionArguments>(
          FunctionArguments&&... function_arguments) -> decltype(auto) {
          return std::invoke(callable,
                             std::forward<FunctionArguments>(function_arguments)...,
                             resource_argument<Resources, resource_local_indices<Resources...>[Is]>(
                               unlimited_resources, bounded_tokens)...);
        },
        std::forward<decltype(arguments)>(arguments));
    }(std::index_sequence_for<Resources...>{});
  }

  template <typename InputMessages, typename Function, typename... Resources>
    requires(not has_bounded<Resources...>)
  struct node_builder<InputMessages, Function, no_outputs_t, std::tuple<Resources...>> {
    using node_t = node_type_for_t<InputMessages, no_outputs_t, std::tuple<Resources...>>;

    template <typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      auto unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources);
      return {
        g,
        concurrency,
        [ft = std::move(ft), node_body = std::move(node_body), unlimited = std::move(unlimited)](
          InputMessages const& messages) mutable -> oneapi::tbb::flow::continue_msg {
          invoke_with_resources<Resources...>(
            node_body, std::forward_as_tuple(ft, messages), std::forward_as_tuple(), unlimited);
          return {};
        }};
    }
  };

  template <typename InputMessages, typename Function, typename... Resources>
    requires(has_bounded<Resources...>)
  struct node_builder<InputMessages, Function, no_outputs_t, std::tuple<Resources...>> {
    using node_t = node_type_for_t<InputMessages, no_outputs_t, std::tuple<Resources...>>;

    template <typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      auto unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources);
      return {
        g,
        concurrency,
        resource_dependencies<Resources...>::bounded_resource_limiters(resources),
        [ft = std::move(ft), node_body = std::move(node_body), unlimited = std::move(unlimited)](
          InputMessages const& messages, auto&, auto&... resource_tokens) mutable {
          invoke_with_resources<Resources...>(node_body,
                                              std::forward_as_tuple(ft, messages),
                                              std::forward_as_tuple(resource_tokens...),
                                              unlimited);
        }};
    }
  };

  template <typename InputMessages, typename Function, typename Output, typename... Resources>
    requires(not has_bounded<Resources...>)
  struct node_builder<InputMessages, Function, std::tuple<Output>, std::tuple<Resources...>> {
    using node_t = node_type_for_t<InputMessages, std::tuple<Output>, std::tuple<Resources...>>;

    template <typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      auto unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources);
      return {
        g,
        concurrency,
        [ft = std::move(ft), node_body = std::move(node_body), unlimited = std::move(unlimited)](
          InputMessages const& messages) mutable {
          return invoke_with_resources<Resources...>(
            node_body, std::forward_as_tuple(ft, messages), std::forward_as_tuple(), unlimited);
        }};
    }

    static tbb::flow::sender<Output>& output_port(node_t& node) { return node; }
  };

  template <typename InputMessages, typename Function, typename Output, typename... Resources>
    requires(has_bounded<Resources...>)
  struct node_builder<InputMessages, Function, std::tuple<Output>, std::tuple<Resources...>> {
    using node_t = node_type_for_t<InputMessages, std::tuple<Output>, std::tuple<Resources...>>;

    template <typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      auto unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources);
      return {
        g,
        concurrency,
        resource_dependencies<Resources...>::bounded_resource_limiters(resources),
        [ft = std::move(ft), node_body = std::move(node_body), unlimited = std::move(unlimited)](
          InputMessages const& messages, auto& ports, auto&... resource_tokens) mutable {
          std::get<0>(ports).try_put(
            invoke_with_resources<Resources...>(node_body,
                                                std::forward_as_tuple(ft, messages),
                                                std::forward_as_tuple(resource_tokens...),
                                                unlimited));
        }};
    }

    static tbb::flow::sender<Output>& output_port(node_t& node)
    {
      return tbb::flow::output_port<0>(node);
    }
  };

  template <typename InputMessages, typename Function, typename... Outputs, typename... Resources>
    requires(not has_bounded<Resources...>)
  struct node_builder<InputMessages,
                      Function,
                      multifunction_outputs<Outputs...>,
                      std::tuple<Resources...>> {
    using node_t =
      node_type_for_t<InputMessages, multifunction_outputs<Outputs...>, std::tuple<Resources...>>;

    template <typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      auto unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources);
      return {
        g,
        concurrency,
        [ft = std::move(ft), node_body = std::move(node_body), unlimited = std::move(unlimited)](
          InputMessages const& messages, auto& ports) mutable {
          invoke_with_resources<Resources...>(node_body,
                                              std::forward_as_tuple(ft, messages, ports),
                                              std::forward_as_tuple(),
                                              unlimited);
        }};
    }
  };

  template <typename InputMessages, typename Function, typename... Outputs, typename... Resources>
    requires(has_bounded<Resources...>)
  struct node_builder<InputMessages,
                      Function,
                      multifunction_outputs<Outputs...>,
                      std::tuple<Resources...>> {
    using node_t =
      node_type_for_t<InputMessages, multifunction_outputs<Outputs...>, std::tuple<Resources...>>;

    template <typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      auto unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources);
      return {
        g,
        concurrency,
        resource_dependencies<Resources...>::bounded_resource_limiters(resources),
        [ft = std::move(ft), node_body = std::move(node_body), unlimited = std::move(unlimited)](
          InputMessages const& messages, auto& ports, auto&... resource_tokens) mutable {
          invoke_with_resources<Resources...>(node_body,
                                              std::forward_as_tuple(ft, messages, ports),
                                              std::forward_as_tuple(resource_tokens...),
                                              unlimited);
        }};
    }
  };
}

#endif // PHLEX_CORE_NODE_BUILDER_HPP
