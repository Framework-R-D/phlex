#ifndef PHLEX_CORE_NODE_BUILDER_HPP
#define PHLEX_CORE_NODE_BUILDER_HPP

#include "phlex/core/resource_api.hpp"

#include <gsl/span>
#include <oneapi/tbb/flow_graph.h>

#include <array>
#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>

namespace phlex::detail {

  // For each resource, in declaration order, the index it would occupy within just the
  // unlimited or just the serialized subsequence, so a single pass can pick its argument
  // from the right tuple.
  template <typename... Resources>
  inline constexpr auto resource_local_indices = [] {
    std::array<std::size_t, sizeof...(Resources)> indices{};
    // The following variables are incremented in the fold expression below.
    // clang-tidy does not recognize this, so we suppress the warning about const correctness.
    // NOLINTBEGIN(misc-const-correctness)
    std::size_t serialized = 0;
    std::size_t unlimited = 0;
    std::size_t i = 0;
    // NOLINTEND(misc-const-correctness)
    // gsl::at used to avoid cppcoreguidelines-pro-bounds-constant-array-index clang-tidy warning.
    ((gsl::at(indices, i++) = unlimited_resource<Resources> ? unlimited++ : serialized++), ...);
    return indices;
  }();

  // Return either the unlimited resource (from the cached tuple) or the serialized-resource token
  // (from the per-call tuple).
  template <typename Resource, std::size_t LocalIndex>
  decltype(auto) resource_argument(auto const& unlimited_resources, auto&& serialized_tokens)
  {
    if constexpr (unlimited_resource<Resource>) {
      return std::get<LocalIndex>(unlimited_resources);
    } else {
      return std::get<LocalIndex>(serialized_tokens);
    }
  }

  // Reassembles Resources... in declaration order for the node body, drawing each argument
  // from either the per-call serialized_tokens (TBB-supplied) or the cached unlimited_resources.
  template <typename... Resources>
  decltype(auto) invoke_with_resources(auto const& callable,
                                       auto&& arguments,
                                       auto&& serialized_tokens,
                                       auto const& unlimited_resources)
  {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
      return std::apply(
        [&]<typename... FunctionArguments>(
          FunctionArguments&&... function_arguments) -> decltype(auto) {
          return std::invoke(callable,
                             std::forward<FunctionArguments>(function_arguments)...,
                             resource_argument<Resources, resource_local_indices<Resources...>[Is]>(
                               unlimited_resources, serialized_tokens)...);
        },
        std::forward<decltype(arguments)>(arguments));
    }(std::index_sequence_for<Resources...>{});
  }

  using no_outputs_t = std::tuple<>;
  using no_serialized_resources_t = std::tuple<>;

  template <typename... Outputs>
  struct multifunction_outputs {};

  // Selects and constructs the TBB node that matches an algorithm's output shape and resource
  // dependencies. Specializations use function_node or multifunction_node when all resources are
  // unlimited, and resource_limited_node when any resource is serialized. They also adapt each
  // node body's construction interface and provide output_port() only for node shapes with outputs.
  template <typename InputMessages, typename Outputs, typename Resources>
  struct node_builder;

  // Specialization for an observer or a fold (no serialized resources)
  template <typename InputMessages, typename... Resources>
    requires(not has_serialized<Resources...>)
  struct node_builder<InputMessages, no_outputs_t, std::tuple<Resources...>> {
    using node_t = tbb::flow::function_node<InputMessages>;

    template <typename Function, typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      return {
        g,
        concurrency,
        [ft = std::move(ft),
         node_body = std::move(node_body),
         unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources)](
          InputMessages const& messages) mutable -> oneapi::tbb::flow::continue_msg {
          invoke_with_resources<Resources...>(
            node_body, std::forward_as_tuple(ft, messages), no_serialized_resources_t{}, unlimited);
          return {};
        }};
    }
  };

  // Specialization for an observer or a fold (serialized resources)
  template <typename InputMessages, typename... Resources>
    requires(has_serialized<Resources...>)
  struct node_builder<InputMessages, no_outputs_t, std::tuple<Resources...>> {
    using node_t = tbb::flow::resource_limited_node<InputMessages, no_outputs_t>;

    template <typename Function, typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      return {
        g,
        concurrency,
        resource_dependencies<Resources...>::serialized_resource_limiters(resources),
        [ft = std::move(ft),
         node_body = std::move(node_body),
         unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources)](
          InputMessages const& messages, no_outputs_t&, auto&... resource_tokens) mutable {
          invoke_with_resources<Resources...>(node_body,
                                              std::forward_as_tuple(ft, messages),
                                              std::forward_as_tuple(resource_tokens...),
                                              unlimited);
        }};
    }
  };

  // Specialization for a transform or a predicate (no serialized resources)
  template <typename InputMessages, typename Output, typename... Resources>
    requires(not has_serialized<Resources...>)
  struct node_builder<InputMessages, std::tuple<Output>, std::tuple<Resources...>> {
    using node_t = tbb::flow::function_node<InputMessages, Output>;

    template <typename Function, typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      return {
        g,
        concurrency,
        [ft = std::move(ft),
         node_body = std::move(node_body),
         unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources)](
          InputMessages const& messages) mutable {
          return invoke_with_resources<Resources...>(
            node_body, std::forward_as_tuple(ft, messages), no_serialized_resources_t{}, unlimited);
        }};
    }

    static tbb::flow::sender<Output>& output_port(node_t& node) { return node; }
  };

  // Specialization for a transform or a predicate (serialized resources)
  template <typename InputMessages, typename Output, typename... Resources>
    requires(has_serialized<Resources...>)
  struct node_builder<InputMessages, std::tuple<Output>, std::tuple<Resources...>> {
    using node_t = tbb::flow::resource_limited_node<InputMessages, std::tuple<Output>>;

    template <typename Function, typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      return {
        g,
        concurrency,
        resource_dependencies<Resources...>::serialized_resource_limiters(resources),
        [ft = std::move(ft),
         node_body = std::move(node_body),
         unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources)](
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

  // Specialization for an unfold (no serialized resources)
  template <typename InputMessages, typename... Outputs, typename... Resources>
    requires(not has_serialized<Resources...>)
  struct node_builder<InputMessages, multifunction_outputs<Outputs...>, std::tuple<Resources...>> {
    using node_t = tbb::flow::multifunction_node<InputMessages, std::tuple<Outputs...>>;

    template <typename Function, typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      return {g,
              concurrency,
              [ft = std::move(ft),
               node_body = std::move(node_body),
               unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(
                 resources)](InputMessages const& messages, auto& ports) mutable {
                invoke_with_resources<Resources...>(node_body,
                                                    std::forward_as_tuple(ft, messages, ports),
                                                    no_serialized_resources_t{},
                                                    unlimited);
              }};
    }
  };

  // Specialization for an unfold (serialized resources)
  template <typename InputMessages, typename... Outputs, typename... Resources>
    requires(has_serialized<Resources...>)
  struct node_builder<InputMessages, multifunction_outputs<Outputs...>, std::tuple<Resources...>> {
    using node_t = tbb::flow::resource_limited_node<InputMessages, std::tuple<Outputs...>>;

    template <typename Function, typename NodeBody>
    static node_t make(tbb::flow::graph& g,
                       std::size_t concurrency,
                       resource_catalog& resources,
                       Function ft,
                       NodeBody node_body)
    {
      return {
        g,
        concurrency,
        resource_dependencies<Resources...>::serialized_resource_limiters(resources),
        [ft = std::move(ft),
         node_body = std::move(node_body),
         unlimited = resource_dependencies<Resources...>::unlimited_resource_accesses(resources)](
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
