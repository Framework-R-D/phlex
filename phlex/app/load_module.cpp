#include "phlex/app/load_module.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/framework_graph.hpp"
#include "phlex/driver.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include "boost/algorithm/string.hpp"
#include "boost/dll/shared_library.hpp"
#include "boost/json.hpp"

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

using namespace std::string_literals;

namespace phlex::detail {

  namespace {
    constexpr std::string_view pymodule_name{"pymodule"};

    // The shared_library member in each wrapper struct keeps the loaded .so
    // alive for the lifetime of the wrapper.  If it goes out of scope, the
    // library is unloaded and the stored function pointer becomes invalid.
    struct module_plugin {
      boost::dll::shared_library lib;
      internal::module_creator_t* fn{};

      void operator()(module_graph_proxy<void_tag> proxy, configuration const& config) const
      {
        fn(std::move(proxy), config);
      }
    };

    struct source_plugin {
      boost::dll::shared_library lib;
      internal::source_creator_t* fn{};

      void operator()(source_bundle bundle, configuration const& config) const
      {
        fn(bundle, config);
      }
    };

    struct driver_plugin {
      boost::dll::shared_library lib;
      internal::driver_shim_t* fn{};

      void operator()(driver_proxy proxy, configuration const& config, driver_bundle* out) const
      {
        fn(proxy, config, out);
      }
    };

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    std::vector<module_plugin> create_module;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    std::vector<source_plugin> create_source;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    std::optional<driver_plugin> create_driver;

    template <typename creator_t>
    std::pair<boost::dll::shared_library, creator_t*> plugin_loader(std::string const& spec,
                                                                    std::string const& symbol_name)
    {
      // Called during single-threaded graph construction
      char const* plugin_path_ptr =
        std::getenv("PHLEX_PLUGIN_PATH"); // NOLINT(concurrency-mt-unsafe)
      if (!plugin_path_ptr)
        throw std::runtime_error("PHLEX_PLUGIN_PATH has not been set.");

      std::vector<std::string> subdirs;
      boost::split(subdirs, plugin_path_ptr, boost::is_any_of(":"));

      // FIXME: Need to test to ensure that first match wins.
      for (auto const& subdir : subdirs) {
        std::filesystem::path shared_library_path{subdir};
        shared_library_path /= "lib" + spec + ".so";
        if (exists(shared_library_path)) {
          // Load pymodule with rtld_global to make Python symbols available to extension modules
          // (e.g., NumPy). Load all other plugins with rtld_local (default) to avoid symbol collisions.
          auto const load_mode = (spec == pymodule_name) ? boost::dll::load_mode::rtld_global
                                                         : boost::dll::load_mode::default_mode;
          boost::dll::shared_library lib{shared_library_path, load_mode};
          auto* fn = &lib.get<creator_t>(symbol_name);
          return {std::move(lib), fn};
        }
      }
      throw std::runtime_error("Could not locate library with specification '"s + spec +
                               "' in any directories on PHLEX_PLUGIN_PATH."s);
    }
  }

  namespace internal {
    boost::json::object adjust_config(std::string const& label, boost::json::object raw_config)
    {
      raw_config["module_label"] = label;

      // Automatically specify the 'pymodule' Phlex plugin if the 'py' parameter is specified
      if (auto const* py = raw_config.if_contains("py")) {
        if (auto const* cpp = raw_config.if_contains("cpp")) {
          std::string msg = fmt::format("Both 'cpp' and 'py' parameters specified for {}", label);
          if (auto const* cpp_value = cpp->if_string()) {
            msg += fmt::format("\n  - cpp: {}", std::string_view(*cpp_value));
          }
          if (auto const* py_value = py->if_string()) {
            msg += fmt::format("\n  - py: {}", std::string_view(*py_value));
          }
          throw std::runtime_error(msg);
        }
        raw_config["cpp"] = pymodule_name;
      }

      return raw_config;
    }
  }

  void load_module(framework_graph& g, std::string const& label, boost::json::object raw_config)
  {
    auto const adjusted_config = internal::adjust_config(label, std::move(raw_config));

    auto const& spec = value_to<std::string>(adjusted_config.at("cpp"));
    auto [lib, fn] = plugin_loader<internal::module_creator_t>(spec, "create_module");
    auto& creator = create_module.emplace_back(module_plugin{std::move(lib), fn});

    configuration const config{adjusted_config};
    creator(g.module_proxy(config), config);
  }

  void load_source(framework_graph& g, std::string const& label, boost::json::object raw_config)
  {
    auto const adjusted_config = internal::adjust_config(label, std::move(raw_config));

    auto const& spec = value_to<std::string>(adjusted_config.at("cpp"));
    auto [lib, fn] = plugin_loader<internal::source_creator_t>(spec, "create_source");
    auto& creator = create_source.emplace_back(source_plugin{std::move(lib), fn});

    // FIXME: Should probably use the parameter name (e.g.) 'plugin_label' instead of
    //        'module_label', but that requires adjusting other parts of the system
    //        (e.g. make_algorithm_name).
    // adjusted_config["module_label"] = label;       // already set by adjust_config

    configuration const config{adjusted_config};
    creator(g.source_proxy(config), config);
  }

  void load_driver(framework_graph& g, boost::json::object const& raw_config)
  {
    configuration const config{raw_config};
    auto const& spec = config.get<std::string>("cpp");
    auto const required_sources = config.get<std::vector<std::string>>("uses_sources", {});
    // False positive: clang-analyzer cannot trace ownership through Boost's is_any_of<char>
    // internal reference counting in classification.hpp.
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks,clang-analyzer-cplusplus.NewDelete)
    auto [lib, fn] = plugin_loader<internal::driver_shim_t>(spec, "create_driver");
    create_driver.emplace(driver_plugin{std::move(lib), fn});
    driver_bundle result;
    (*create_driver)(g.driver_proxy(required_sources), config, &result);
    g.add_driver(result);
  }
}
