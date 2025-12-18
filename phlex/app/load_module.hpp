#ifndef PHLEX_APP_LOAD_MODULE_HPP
#define PHLEX_APP_LOAD_MODULE_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/driver.hpp"

#include "boost/json.hpp"

#include <functional>

namespace phlex::experimental {
  namespace detail {
    // Adjust_config adds the module_label as a parameter, and it checks if the 'py'
    // parameter exists, inserting the 'cpp: "pymodule"' configuration if necessary.
    boost::json::object adjust_config(std::string const& label, boost::json::object raw_config);
  }

  void load_module(framework_graph& g, std::string const& label, boost::json::object config);
  void load_source(framework_graph& g, std::string const& label, boost::json::object config);
  detail::next_index_t load_driver(boost::json::object const& config);
}

#endif // PHLEX_APP_LOAD_MODULE_HPP
