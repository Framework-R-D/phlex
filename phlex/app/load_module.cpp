#include "phlex/app/load_module.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/framework_graph.hpp"
#include "phlex/driver.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include "boost/algorithm/string.hpp"
#include "boost/dll/import.hpp"
#include "boost/json.hpp"

#include <functional>
#include <string>

using namespace std::string_literals;

namespace phlex::experimental {

  namespace {
    // If factory function goes out of scope, then the library is unloaded...and that's
    // bad.
    std::vector<std::function<detail::module_creator_t>> create_module;
    std::vector<std::function<detail::source_creator_t>> create_source;
    std::function<detail::driver_creator_t> create_driver;

    template <typename creator_t>
    std::function<creator_t> plugin_loader(std::string const& spec, std::string const& symbol_name)
    {
      char const* plugin_path_ptr = std::getenv("PHLEX_PLUGIN_PATH");
      if (!plugin_path_ptr)
        throw std::runtime_error("PHLEX_PLUGIN_PATH has not been set.");

      using namespace boost;
      std::vector<std::string> subdirs;
      split(subdirs, plugin_path_ptr, is_any_of(":"));

      // FIXME: Need to test to ensure that first match wins.
      for (auto const& subdir : subdirs) {
        std::filesystem::path shared_library_path{subdir};
        shared_library_path /= "lib" + spec + ".so";
        if (exists(shared_library_path)) {
          return dll::import_alias<creator_t>(shared_library_path, symbol_name);
        }
      }
      throw std::runtime_error("Could not locate library with specification '"s + spec +
                               "' in any directories on PHLEX_PLUGIN_PATH."s);
    }
  }

  namespace detail {
    boost::json::object adjust_config(std::string const& label, boost::json::object raw_config)
    {
      raw_config["module_label"] = label;

      // Automatically specify the 'pymodule' Phlex plugin if the 'py' parameter is specified
      if (auto const* py = raw_config.if_contains("py")) {
        if (auto const* cpp = raw_config.if_contains("cpp")) {
          std::string msg = fmt::format("Both 'cpp' and 'py' parameters specified for {}", label);
          if (auto const* cpp_value = cpp->if_string()) {
            msg += fmt::format("\n  - cpp: {}", *cpp_value);
          }
          if (auto const* py_value = py->if_string()) {
            msg += fmt::format("\n  - py: {}", *py_value);
          }
          throw std::runtime_error(msg);
        }
        raw_config["cpp"] = "pymodule";
      }

      return raw_config;
    }
  }

  void load_module(framework_graph& g, std::string const& label, boost::json::object raw_config)
  {
    auto const adjusted_config = detail::adjust_config(label, std::move(raw_config));

    auto const& spec = value_to<std::string>(adjusted_config.at("cpp"));
    auto& creator =
      create_module.emplace_back(plugin_loader<detail::module_creator_t>(spec, "create_module"));

    configuration const config{adjusted_config};
    creator(g.module_proxy(config), config);
  }

  void load_source(framework_graph& g, std::string const& label, boost::json::object raw_config)
  {
    auto const& spec = value_to<std::string>(raw_config.at("cpp"));
    auto& creator =
      create_source.emplace_back(plugin_loader<detail::source_creator_t>(spec, "create_source"));

    // FIXME: Should probably use the parameter name (e.g.) 'plugin_label' instead of
    //        'module_label', but that requires adjusting other parts of the system
    //        (e.g. make_algorithm_name).
    raw_config["module_label"] = label;

    configuration const config{raw_config};
    creator(g.source_proxy(config), config);
  }

  detail::next_index_t load_driver(boost::json::object const& raw_config)
  {
    configuration const config{raw_config};
    auto const& spec = config.get<std::string>("cpp");
    create_driver = plugin_loader<detail::driver_creator_t>(spec, "create_driver");
    return create_driver(config);
  }
}
