#include "phlex/core/framework_graph.hpp"

int main()
{
  auto g = phlex::detail::framework_graph::without_driver("test");
  g.fold("accumulator_only", [](unsigned int&) {}, phlex::concurrency::serial);
}
