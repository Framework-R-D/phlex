#include "phlex/core/products_consumer.hpp"
#include "phlex/core/product_registry.hpp"

#include "spdlog/spdlog.h"

namespace {
  using phlex::experimental::identifier;
  std::vector<identifier> layers_from(identifier const& plugin,
                                      identifier const& algorithm,
                                      phlex::product_queries const& queries,
                                      phlex::experimental::product_registry const& registry)
  {
    spdlog::debug(
      "Determining layers for {}:{} from input queries [{}]", plugin, algorithm, queries);
    std::vector<identifier> result;
    result.reserve(queries.size());
    for (auto const& query : queries) {
      result.push_back(registry.lookup(query).layer);
    }
    return result;
  }
}

namespace phlex::experimental {

  products_consumer::products_consumer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_queries input_products,
                                       product_registry const& registry) :
    consumer{std::move(name), std::move(predicates)},
    input_products_{std::move(input_products)},
    registry_{&registry}
  {
    spdlog::debug("Creating node for {}:{}", plugin(), algorithm());
  }

  products_consumer::~products_consumer() = default;

  std::size_t products_consumer::num_inputs() const { return input().size(); }

  tbb::flow::receiver<message>& products_consumer::port(product_query const& input_product)
  {
    return port_for(input_product);
  }

  product_queries const& products_consumer::input() const noexcept { return input_products_; }
  std::vector<identifier> const& products_consumer::layers() const noexcept
  {
    if (layers_.empty() && !input_products_.empty()) {
      layers_ = layers_from(plugin(), algorithm(), input_products_, *registry_);
    }
    return layers_;
  }
}
