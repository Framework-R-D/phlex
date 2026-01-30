#include "phlex/core/input_arguments.hpp"

#include "fmt/format.h"

#include <stdexcept>

namespace phlex::experimental::detail {
  void verify_no_duplicate_input_products(std::string const& algorithm_name,
                                          product_queries input_queries)
  {
    for (std::size_t i = 0; i < input_queries.size(); ++i) {
      for (std::size_t j = i + 1; j < input_queries.size(); ++j) {
        if (input_queries[i].match(input_queries[j]) || input_queries[j].match(input_queries[i])) {
          throw std::runtime_error(
            fmt::format("The algorithm {} has at least one duplicate input:\n- {}\n- {}\n",
                        algorithm_name,
                        input_queries[i].to_string(),
                        input_queries[j].to_string()));
        }
      }
    }
  }
}
