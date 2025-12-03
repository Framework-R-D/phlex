#include "phlex/core/registrar.hpp"

#include "fmt/format.h"

namespace phlex::experimental::detail {

  void verify_layers(std::vector<std::string>& errors,
                     algorithm_name const& name,
                     std::span<product_query const> queries)
  {
    std::vector<std::string> malformed_queries;
    std::string err_msg;
    for (auto const& query : queries) {
      if (query.family.empty()) {
        malformed_queries.push_back(query.to_string());
      }
    }

    if (malformed_queries.empty()) {
      return;
    }

    errors.push_back(fmt::format("Node '{}' has one or more product specifications that do not "
                                 "include a data layer: {}",
                                 name.full(),
                                 malformed_queries));
  }

  void add_to_error_messages(std::vector<std::string>& errors, std::string const& name)
  {
    errors.push_back(fmt::format("Node with name '{}' already exists", name));
  }
}
