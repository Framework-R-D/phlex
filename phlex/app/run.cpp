#include "phlex/app/run.hpp"
#include "phlex/app/load_module.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/core/framework_graph.hpp"

using namespace std::string_literals;

namespace {
  auto object_decorate_exception(boost::json::object const& obj, std::string const& key)
  try {
    return obj.at(key).as_object();
  } catch (std::exception const& e) {
    throw std::runtime_error("Error retrieving parameter '" + key + "':\n" + e.what());
  }
}

namespace phlex::experimental {
  void run(boost::json::object const& configurations, int const max_parallelism)
  {
    auto const driver_config = object_decorate_exception(configurations, "driver");
    framework_graph g{load_driver(driver_config), max_parallelism};
    auto const module_configs = object_decorate_exception(configurations, "modules");
    for (auto const& [key, value] : module_configs) {
      load_module(g, key, value.as_object());
    }
    g.execute();
  }
}
