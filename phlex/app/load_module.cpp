#include "phlex/app/load_module.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/framework_graph.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include "boost/dll/import.hpp"

#include <bits/basic_string.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value_to.hpp>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::string_literals;

namespace phlex::experimental {

  namespace {
    // If factory function goes out of scope, then the library is unloaded...and that's
    // bad.
    std::vector<std::function<detail::module_creator_t>> create_module;
    std::function<detail::source_creator_t> create_source;

    template <typename creator_t>
    auto plugin_loader(std::string const& spec, std::string const& symbol_name) -> std::function<creator_t>
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

  void load_module(framework_graph& g, std::string const& label, boost::json::object raw_config)
  {
    auto const& spec = value_to<std::string>(raw_config.at("plugin"));
    auto& creator =
      create_module.emplace_back(plugin_loader<detail::module_creator_t>(spec, "create_module"));
    raw_config["module_label"] = label;

    configuration const config{raw_config};
    auto module_proxy = g.proxy(config);
    creator(module_proxy, config);
  }

  auto load_source(boost::json::object const& raw_config) -> detail::next_store_t
  {
    configuration const config{raw_config};
    auto const& spec = config.get<std::string>("plugin");
    create_source = plugin_loader<detail::source_creator_t>(spec, "create_source");
    return create_source(config);
  }
}
