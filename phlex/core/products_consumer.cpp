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
    result.shrink_to_fit();
    return result;
  }
}

namespace phlex::detail {

  products_consumer::products_consumer(phlex::experimental::algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_selectors input_products,
                                       bool always_require_layers) :
    consumer{std::move(name), std::move(predicates)},
    input_products_{std::move(input_products)},
    layers_{layers_from(input_products_)}
  {
    using namespace phlex::experimental::literals;
    if (always_require_layers || input_products_.size() > 1) {
      std::vector<std::string> err_selectors{};
      for (auto const& p : input_products_) {
        if (!p.layer) {
          err_selectors.push_back(p.to_string());
        }
      }
      if (!err_selectors.empty()) {
        std::string error = fmt::format(
          "Product selectors in multi-input algorithms (here: {}) must define their layers:\n"
          "  (Only invalid selectors are listed)\n{}",
          this->name().to_string(),
          bulleted_list(err_selectors));
        throw std::runtime_error(error);
      }
    }
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
