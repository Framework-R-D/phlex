#include "phlex/core/products_consumer.hpp"

namespace {
  std::vector<phlex::experimental::identifier> layers_from(phlex::product_queries const& queries)
  {
    std::vector<phlex::experimental::identifier> result;
    result.reserve(queries.size());
    for (auto const& query : queries) {
      result.push_back(query.layer);
    }
    return result;
  }
}

namespace phlex::experimental {

  products_consumer::products_consumer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_queries input_products) :
    consumer{std::move(name), std::move(predicates)},
    input_products_{std::move(input_products)},
    layers_{layers_from(input_products_)}
  {
  }

  products_consumer::~products_consumer() = default;

  std::size_t products_consumer::num_inputs() const { return input().size(); }

  tbb::flow::receiver<message>& products_consumer::port(product_query const& product_label)
  {
    return port_for(product_label);
  }

  product_queries const& products_consumer::input() const noexcept { return input_products_; }
  std::vector<identifier> const& products_consumer::layers() const noexcept { return layers_; }
}
