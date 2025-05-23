#ifndef phlex_app_load_module_hpp
#define phlex_app_load_module_hpp

#include "phlex/core/fwd.hpp"
#include "phlex/source.hpp"

#include "boost/json.hpp"

#include <functional>

namespace phlex::experimental {
  void load_module(framework_graph& g, std::string const& label, boost::json::object config);
  detail::next_store_t load_source(boost::json::object const& config);
}

#endif // phlex_app_load_module_hpp
