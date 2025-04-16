#include "phlex/core/products_consumer.hpp"

namespace phlex::experimental {

  products_consumer::products_consumer(algorithm_name name, std::vector<std::string> predicates) :
    consumer{std::move(name), std::move(predicates)}
  {
  }

  products_consumer::~products_consumer() = default;

  std::size_t products_consumer::num_inputs() const { return input().size(); }

  tbb::flow::receiver<message>& products_consumer::port(specified_label const& product_label)
  {
    return port_for(product_label);
  }

}
