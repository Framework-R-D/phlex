#include "phlex/module.hpp"

#include <vector>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  auto const input_creator = config.get<std::string>("input_creator", "add_cov");

  m.transform("passthrough_sums", [](std::vector<int> const& values) { return values; })
    .input_family(product_selector{.creator = phlex::experimental::identifier{input_creator},
                                   .layer = "event",
                                   .suffix = "sums"})
    .output_product_suffixes("sums");
}
