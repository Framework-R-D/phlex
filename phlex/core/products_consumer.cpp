#include "phlex/core/products_consumer.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"
#include <cstddef>
#include <oneapi/tbb/flow_graph.h>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {

  products_consumer::products_consumer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       specified_labels input_products) :
    consumer{std::move(name), std::move(predicates)}, input_products_{std::move(input_products)}
  {
  }

  products_consumer::~products_consumer() = default;

  auto products_consumer::num_inputs() const -> std::size_t { return input().size(); }

  auto products_consumer::port(specified_label const& product_label) -> tbb::flow::receiver<message>&
  {
    return port_for(product_label);
  }

  auto products_consumer::input() const noexcept -> specified_labels const& { return input_products_; }
}
