#include "phlex/app/run.hpp"

#include "phlex/app/load_module.hpp"
#include "phlex/core/framework_graph.hpp"

#include <boost/json/object.hpp>
#include <fmt/format.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <stdexcept>
#include <string>
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
  void run(boost::json::object const& configurations, overridable_configuration const& overridables)
  {
    if (!overridables.stage) {
      throw std::runtime_error(
        "No stage name was specified. Provide one using the '--stage <name>' program option "
        "or the top-level configuration parameter 'stage: \"<name>\"'.");
    }
    // FIXME: Eventually, make it impossible to create a stage name that is empty or "CURRENT".
    auto stage = overridables.stage.value();
    if (stage.empty()) {
      throw std::runtime_error(
        "The stage name cannot be empty. Provide a non-empty name using the '--stage <name>' "
        "program option or the top-level configuration parameter 'stage: \"<name>\"'.");
    }
    if (stage == "CURRENT") {
      throw std::runtime_error(
        "'CURRENT' is reserved and cannot be used as a stage name. Provide a different name "
        "using the '--stage <name>' program option or the top-level configuration parameter "
        "'stage: \"<name>\"'.");
    }

    // Capture the default logger so we can restore it after loading the plugins, which may
    // change the default logger.
    auto const default_logger = spdlog::default_logger();
    spdlog::cfg::load_env_levels();

    auto g = framework_graph::without_driver(std::move(stage), overridables.max_parallelism);

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

    spdlog::set_default_logger(default_logger);
    g.execute();
  }
}
