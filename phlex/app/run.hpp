#ifndef PHLEX_APP_RUN_HPP
#define PHLEX_APP_RUN_HPP

#include "phlex/run_phlex_export.hpp"

#include <boost/json.hpp>
#include <oneapi/tbb/info.h>

#include <optional>
#include <string>

namespace phlex::detail {
  struct overridable_configuration {
    std::optional<std::string> stage;
    int max_parallelism{oneapi::tbb::info::default_concurrency()};
  };

  RUN_PHLEX_EXPORT void run(boost::json::object const& configurations,
                            overridable_configuration const& overrides);
}

#endif // PHLEX_APP_RUN_HPP
