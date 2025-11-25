#include "phlex/core/products_consumer.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"

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

  std::size_t products_consumer::num_inputs() const { return input().size(); }

  tbb::flow::receiver<message>& products_consumer::port(specified_label const& product_label)
  {
    return port_for(product_label);
  }

  specified_labels const& products_consumer::input() const noexcept { return input_products_; }
}
