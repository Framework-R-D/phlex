#include "phlex/core/input_arguments.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

namespace phlex::experimental::detail {
  void verify_no_duplicate_input_products(std::string const& algorithm_name,
                                          specified_labels to_sort)
  {
    std::sort(begin(to_sort), end(to_sort));
    std::set unique_and_sorted(begin(to_sort), end(to_sort));
    specified_labels duplicates;
    std::set_difference(begin(to_sort),
                        end(to_sort),
                        begin(unique_and_sorted),
                        end(unique_and_sorted),
                        std::back_inserter(duplicates));
    if (empty(duplicates)) {
      return;
    }

    std::string error =
      fmt::format("Algorithm '{}' uses the following products more than once:\n", algorithm_name);
    for (auto const& label : duplicates) {
      error += fmt::format(" - '{}'\n", label.to_string());
    }
    throw std::runtime_error(error);
  }
}
