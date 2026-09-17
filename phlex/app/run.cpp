#include "phlex/app/run.hpp"

#include "phlex/app/load_module.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/core/framework_graph.hpp"

#include "fmt/format.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace std::string_literals;

namespace {
  auto object_decorate_exception(boost::json::object const& obj, std::string const& key)
  try {
    return obj.at(key).as_object();
  } catch (std::exception const& e) {
    throw std::runtime_error(fmt::format("Error retrieving parameter '{}' :\n{}", key, e.what()));
  }
}

namespace phlex::detail {
  void run(boost::json::object const& configurations, overridable_configuration const& overrides)
  {
    if (!overrides.stage) {
      throw std::runtime_error("Must provide a 'stage' name.");
    }

    auto g = framework_graph::without_driver(overrides.stage.value(), overrides.max_parallelism);

    boost::json::object resource_configs;
    if (configurations.contains("resources")) {
      resource_configs = object_decorate_exception(configurations, "resources");
    }

    for (auto const& [key, value] : resource_configs) {
      load_resource(g, key, value.as_object());
    }

    // It is allowed for users to not specify any modules
    boost::json::object module_configs;
    if (configurations.contains("modules")) {
      module_configs = object_decorate_exception(configurations, "modules");
    }

    for (auto const& [key, value] : module_configs) {
      load_module(g, key, value.as_object());
    }

    boost::json::object source_configs;
    if (configurations.contains("sources")) {
      source_configs = object_decorate_exception(configurations, "sources");
    }

    for (auto const& [key, value] : source_configs) {
      load_source(g, key, value.as_object());
    }

    auto const driver_config = object_decorate_exception(configurations, "driver");
    load_driver(g, driver_config);

    g.execute();
  }
}
