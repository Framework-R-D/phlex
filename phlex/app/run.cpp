#include "phlex/app/run.hpp"
#include "phlex/app/load_module.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/core/framework_graph.hpp"

using namespace std::string_literals;

namespace phlex::experimental {
  void run(boost::json::object const& configurations,
           std::optional<std::string> dot_file,
           int const max_parallelism)
  {
    framework_graph g{load_source(configurations.at("source").as_object()), max_parallelism};
    auto const module_configs = configurations.at("modules").as_object();
    for (auto const& [key, value] : module_configs) {
      load_module(g, key, value.as_object());
    }
    g.execute(dot_file.value_or(""s));
  }
}
