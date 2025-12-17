#ifndef PHLEX_APP_RUN_HPP
#define PHLEX_APP_RUN_HPP

#include "boost/json.hpp"

#include <optional>

namespace phlex::experimental {
  void run(boost::json::object const& configurations, int max_parallelism);
}

#endif // PHLEX_APP_RUN_HPP
