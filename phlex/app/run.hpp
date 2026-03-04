#ifndef PHLEX_APP_RUN_HPP
#define PHLEX_APP_RUN_HPP

#include "run_phlex_export.hpp"

#include "boost/json.hpp"

#include <optional>

namespace phlex::experimental {
  run_phlex_EXPORT void run(boost::json::object const& configurations, int max_parallelism);
}

#endif // PHLEX_APP_RUN_HPP
