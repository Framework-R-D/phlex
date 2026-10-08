#include "phlex/core/glue.hpp"

#include "phlex/configuration.hpp"
#include "phlex/core/node_catalog.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace phlex::detail::internal {
  registration_core::registration_core(configuration const* config,
                                       tbb::flow::graph& graph,
                                       phlex::experimental::identifier const& stage,
                                       node_catalog& nodes,
                                       std::vector<std::string>& errors,
                                       resource_catalog& resources) :
    config_{config},
    graph_{graph},
    stage_{stage},
    nodes_{nodes},
    errors_{errors},
    resources_{resources}
  {
  }

  void registration_core::verify_name(std::string_view name) const
  {
    internal::verify_name(name, config_);
  }

  output_api registration_core::output(std::string_view name, output_function_t f, concurrency c)
  {
    return output_api{
      nodes_.registrar_for<declared_output_ptr>(errors_), config_, name, graph_, std::move(f), c};
  }

  void registration_core::insert_source(std::string_view name, std::unique_ptr<source> source)
  {
    auto [_, inserted] = nodes_.sources.try_emplace(std::string{name}, std::move(source));
    if (not inserted) {
      internal::add_to_error_messages(errors_, "Source", std::string{name});
    }
  }

  void verify_name(std::string_view name, configuration const* config)
  {
    if (not name.empty()) {
      return;
    }

    std::string msg{"Cannot specify algorithm with no name"};
    std::string const module = config ? config->get<std::string>("module_label") : "";
    if (!module.empty()) {
      msg += " (module: '";
      msg += module;
      msg += "')";
    }
    throw std::runtime_error{msg};
  }
}
