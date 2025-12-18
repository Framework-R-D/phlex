#ifndef PHLEX_APP_LOAD_MODULE_HPP
#define PHLEX_APP_LOAD_MODULE_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/source.hpp"

#include "boost/json.hpp"

#include <functional>

namespace phlex::experimental {
  void load_module(framework_graph& g, std::string const& label, boost::json::object config);
  detail::next_index_t load_source(boost::json::object const& config);
}

#endif // PHLEX_APP_LOAD_MODULE_HPP
