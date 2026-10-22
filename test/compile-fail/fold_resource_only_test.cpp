#include "phlex/core/framework_graph.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"

#include <array>
#include <tuple>

namespace {
  struct fold_resource {
    using token_type = fold_resource const*;
  };
}

int main()
{
  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_serialized_resource<fold_resource>();
  g.fold(
     "resource_only", [](unsigned int&, fold_resource const*) {}, phlex::concurrency::serial)
    .input_family(std::array<phlex::product_selector, 0>{},
                  std::tuple{phlex::resource<fold_resource>{}});
}
