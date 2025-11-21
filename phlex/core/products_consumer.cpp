#include "phlex/core/products_consumer.hpp"

namespace phlex::experimental {

  products_consumer::products_consumer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_queries input_products) :
    consumer{std::move(name), std::move(predicates)}, input_products_{std::move(input_products)}
  {
  }

  products_consumer::~products_consumer() = default;

  std::size_t products_consumer::num_inputs() const { return input().size(); }

  tbb::flow::receiver<message>& products_consumer::port(product_query const& product_label)
  {
    return port_for(product_label);
  }

  product_queries const& products_consumer::input() const noexcept { return input_products_; }
}
