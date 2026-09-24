#include "phlex/configuration.hpp"
#include "phlex/resource.hpp"
#include "test/plugins/resources_for_testing.hpp"

using namespace phlex;
using namespace test::plugins;

namespace {
  using proxy = detail::resources_graph_proxy;

  template <typename T>
  concept exposes_observe =
    requires(T const& resource_proxy) { resource_proxy.observe("invalid", [](int) {}); };

  static_assert(requires(proxy const& r) {
    r.template add_unlimited_resource<configured_resource>(1);
    r.template add_serialized_resource<serialized_resource>();
  });
  static_assert(not exposes_observe<proxy>);
}

PHLEX_REGISTER_RESOURCES(r, config)
{
  r.add_unlimited_resource<configured_resource>(config.get<int>("value"));
  r.add_serialized_resource<serialized_resource>();
}
