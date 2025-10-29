#ifndef TEST_PRODUCTS_FOR_OUTPUT_HPP
#define TEST_PRODUCTS_FOR_OUTPUT_HPP

#include "phlex/model/product_store.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <sstream>

namespace phlex::experimental::test {
  struct products_for_output {
    void save(product_store const& store) const
    {
      std::ostringstream oss;
      oss << "Saving data for store id: " << store.id() << " from source: " << store.source();
      for (auto const& [product_name, _] : store) {
        oss << "\n -> Product name: " << product_name;
      }
      spdlog::debug(oss.str());
    }
  };

}

#endif // TEST_PRODUCTS_FOR_OUTPUT_HPP
