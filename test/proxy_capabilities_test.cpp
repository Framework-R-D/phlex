#include "phlex/core/framework_graph.hpp"
#include "phlex/module.hpp"
#include "phlex/resource.hpp"
#include "phlex/source.hpp"

#include <concepts>
#include <string>
#include <utility>

using namespace phlex;
using namespace phlex::detail;

namespace {
  struct bound_object {};

  struct unlimited_test_resource {};

  struct serialized_test_resource {
    using token_type = serialized_test_resource*;
  };

  struct test_source : source {
    provider_bundles create_providers(product_selector const&) override { return {}; }
  };

  struct test_unfolder {
    explicit test_unfolder(int) {}
    static int initial_value() { return 0; }
  };

  void fold_function [[maybe_unused]] (int&, int);
  void observe_function [[maybe_unused]] (int);
  void output_function [[maybe_unused]] (experimental::product_store const&);
  bool predicate_function [[maybe_unused]] (int);
  int provide_function [[maybe_unused]] (data_cell_index const&);
  int transform_function [[maybe_unused]] (int);
  std::pair<int, int> unfold_function [[maybe_unused]] (int);

  template <typename T>
  concept has_make = requires(T const& proxy) { proxy.template make<bound_object>(); };

  template <typename T>
  concept has_fold = requires(T const& proxy) { proxy.fold("fold", fold_function); };

  template <typename T>
  concept has_observe = requires(T const& proxy) { proxy.observe("observe", observe_function); };

  template <typename T>
  concept has_output = requires(T const& proxy) { proxy.output("output", output_function); };

  template <typename T>
  concept has_predicate =
    requires(T const& proxy) { proxy.predicate("predicate", predicate_function); };

  template <typename T>
  concept has_provide = requires(T const& proxy) { proxy.provide("provide", provide_function); };

  template <typename T>
  concept has_transform =
    requires(T const& proxy) { proxy.transform("transform", transform_function); };

  template <typename T>
  concept has_unfold = requires(T const& proxy) {
    proxy.template unfold<test_unfolder>(
      "unfold", predicate_function, unfold_function, std::string{"event"});
  };

  template <typename T>
  concept has_add_source =
    requires(T const& proxy) { proxy.template add_source<test_source>("source"); };

  template <typename T>
  concept has_add_unlimited_resource =
    requires(T const& proxy) { proxy.template add_unlimited_resource<unlimited_test_resource>(); };

  template <typename T>
  concept has_add_serialized_resource = requires(T const& proxy) {
    proxy.template add_serialized_resource<serialized_test_resource>();
  };

  using module_proxy = module_graph_proxy<void_tag>;
  using bound_module_proxy = module_graph_proxy<bound_object>;
  static_assert(has_make<module_proxy>);
  static_assert(has_fold<module_proxy>);
  static_assert(has_observe<module_proxy>);
  static_assert(has_output<module_proxy>);
  static_assert(has_predicate<module_proxy>);
  static_assert(has_transform<module_proxy>);
  static_assert(has_unfold<module_proxy>);
  static_assert(not has_provide<module_proxy>);
  static_assert(not has_add_source<module_proxy>);
  static_assert(not has_add_unlimited_resource<module_proxy>);
  static_assert(not has_add_serialized_resource<module_proxy>);
  static_assert(not has_make<bound_module_proxy>);
  static_assert(std::same_as<decltype(std::declval<module_proxy const&>().make<bound_object>()),
                             bound_module_proxy>);

  using provider_proxy = providers_graph_proxy<void_tag>;
  using bound_provider_proxy = providers_graph_proxy<bound_object>;
  static_assert(has_make<provider_proxy>);
  static_assert(has_provide<provider_proxy>);
  static_assert(not has_fold<provider_proxy>);
  static_assert(not has_observe<provider_proxy>);
  static_assert(not has_output<provider_proxy>);
  static_assert(not has_predicate<provider_proxy>);
  static_assert(not has_transform<provider_proxy>);
  static_assert(not has_unfold<provider_proxy>);
  static_assert(not has_add_source<provider_proxy>);
  static_assert(not has_add_unlimited_resource<provider_proxy>);
  static_assert(not has_add_serialized_resource<provider_proxy>);
  static_assert(not has_make<bound_provider_proxy>);
  static_assert(std::same_as<decltype(std::declval<provider_proxy const&>().make<bound_object>()),
                             bound_provider_proxy>);

  static_assert(has_add_source<source_graph_proxy>);
  static_assert(not has_make<source_graph_proxy>);
  static_assert(not has_fold<source_graph_proxy>);
  static_assert(not has_observe<source_graph_proxy>);
  static_assert(not has_output<source_graph_proxy>);
  static_assert(not has_predicate<source_graph_proxy>);
  static_assert(not has_provide<source_graph_proxy>);
  static_assert(not has_transform<source_graph_proxy>);
  static_assert(not has_unfold<source_graph_proxy>);
  static_assert(not has_add_unlimited_resource<source_graph_proxy>);
  static_assert(not has_add_serialized_resource<source_graph_proxy>);

  static_assert(not has_make<resources_graph_proxy>);
  static_assert(not has_fold<resources_graph_proxy>);
  static_assert(not has_observe<resources_graph_proxy>);
  static_assert(not has_output<resources_graph_proxy>);
  static_assert(not has_predicate<resources_graph_proxy>);
  static_assert(not has_provide<resources_graph_proxy>);
  static_assert(not has_transform<resources_graph_proxy>);
  static_assert(not has_unfold<resources_graph_proxy>);
  static_assert(not has_add_source<resources_graph_proxy>);
  static_assert(has_add_unlimited_resource<resources_graph_proxy>);
  static_assert(has_add_serialized_resource<resources_graph_proxy>);
  static_assert(requires(resources_graph_proxy const& proxy) {
    proxy.template add_unlimited_resource<unlimited_test_resource>();
    proxy.template add_serialized_resource<serialized_test_resource>();
  });

  static_assert(std::same_as<decltype(std::declval<framework_graph&>().make<bound_object>()),
                             glue<bound_object>>);
  static_assert(not has_output<framework_graph>);
  static_assert(requires(framework_graph& graph) {
    graph.fold("fold", fold_function);
    graph.observe("observe", observe_function);
    graph.predicate("predicate", predicate_function);
    graph.provide("provide", provide_function);
    graph.transform("transform", transform_function);
    graph.template add_source<test_source>("source");
    graph.template add_unlimited_resource<unlimited_test_resource>();
    graph.template add_serialized_resource<serialized_test_resource>();
    graph.template unfold<test_unfolder>(
      "unfold", predicate_function, unfold_function, concurrency::serial, std::string{"event"});
  });
}

int main() {}
