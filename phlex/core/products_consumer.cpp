#include "phlex/core/products_consumer.hpp"

#include "fmt/format.h"

namespace {
  std::vector<phlex::experimental::identifier> layers_from(phlex::product_selectors const& queries)
  {
    using namespace phlex::experimental::literals;
    std::vector<phlex::experimental::identifier> result;
    result.reserve(queries.size());
    for (auto const& query : queries) {
      if (query.layer) {
        result.push_back(query.layer);
      } else {
        result.push_back("*"_id);
      }
    }
    return result;
  }

  void validate_layers(phlex::detail::require_layers layers_required,
                       phlex::product_selectors const& inputs,
                       phlex::experimental::algorithm_name const& algo)
  {
    using namespace phlex::detail;
    if (layers_required == require_layers::never) {
      return;
    }
    if (layers_required == require_layers::multi_input_only && inputs.size() <= 1) {
      return;
    }
    std::vector<std::string> err_selectors{};
    for (auto const& p : inputs) {
      if (!p.layer) {
        err_selectors.push_back(p.to_string());
      }
    }
    if (!err_selectors.empty()) {
      std::string type =
        layers_required == require_layers::always ? "layer-mandatory" : "multi-input";
      std::string error =
        fmt::format("Product selectors in {} algorithm {} must define their layers:\n"
                    "  (Only invalid selectors are listed)\n{}",
                    type,
                    algo.to_string(),
                    bulleted_list(err_selectors));
      throw std::runtime_error(error);
    }
  }
}

namespace phlex::detail {

  products_consumer::products_consumer(phlex::experimental::algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_selectors input_products,
                                       require_layers layers_required) :
    consumer{std::move(name), std::move(predicates)},
    input_products_{std::move(input_products)},
    layers_{layers_from(input_products_)}
  {
    validate_layers(layers_required, input_products_, this->name());
  }

  products_consumer::~products_consumer() = default;

  std::size_t products_consumer::num_inputs() const { return input().size(); }

  tbb::flow::receiver<message>& products_consumer::port(product_selector const& input_product)
  {
    return port_for(input_product);
  }

  product_selectors const& products_consumer::input() const noexcept { return input_products_; }
  std::vector<phlex::experimental::identifier> const& products_consumer::layers() const noexcept
  {
    return layers_;
  }
}
