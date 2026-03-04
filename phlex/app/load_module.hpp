#ifndef PHLEX_APP_LOAD_MODULE_HPP
#define PHLEX_APP_LOAD_MODULE_HPP

#include "run_phlex_export.hpp"

#include "phlex/core/fwd.hpp"
#include "phlex/driver.hpp"

#include "boost/json.hpp"

#include <functional>

namespace phlex::experimental {
  namespace detail {
    // Adjust_config adds the module_label as a parameter, and it checks if the 'py'
    // parameter exists, inserting the 'cpp: "pymodule"' configuration if necessary.
    run_phlex_EXPORT boost::json::object adjust_config(std::string const& label,
                                                       boost::json::object raw_config);
  }

  run_phlex_EXPORT void load_module(framework_graph& g,
                                    std::string const& label,
                                    boost::json::object config);
  run_phlex_EXPORT void load_source(framework_graph& g,
                                    std::string const& label,
                                    boost::json::object config);
  run_phlex_EXPORT detail::next_index_t load_driver(boost::json::object const& config);
}

#endif // PHLEX_APP_LOAD_MODULE_HPP
