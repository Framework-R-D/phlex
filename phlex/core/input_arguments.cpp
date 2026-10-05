#include "phlex/core/input_arguments.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <fmt/format.h>
#include <gsl/pointers>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace phlex::detail::internal {
  gsl::not_null<phlex::experimental::product_specification const*> resolve_product(
    product_selector const& query, phlex::experimental::product_store const& store)
  {
    phlex::experimental::product_specification const* product = nullptr;
    std::size_t matches = 0;
    for (auto const& [spec, _] : store) {
      if (query.match(spec)) {
        product = &spec;
        ++matches;
      }
    }

    if (matches == 0) {
      std::vector<std::string> products;
      products.reserve(store.size());
      for (auto const& [spec, _] : store) {
        products.push_back(spec.to_string());
      }
      throw std::runtime_error(
        fmt::format("No products found matching the query {}\n Store (id {} from {}) contains:\n{}",
                    query,
                    store.index()->to_string(),
                    store.source().to_string(),
                    bulleted_list(products, /*indent=*/4)));
    }

    if (matches > 1) {
      std::vector<std::string> products;
      for (auto const& [spec, _] : store) {
        if (query.match(spec)) {
          products.push_back(spec.to_string());
        }
      }
      throw std::runtime_error(fmt::format("Multiple products found matching the query {}:\n{}",
                                           query,
                                           bulleted_list(products, /*indent=*/4)));
    }

    return gsl::make_not_null(product);
  }
}
