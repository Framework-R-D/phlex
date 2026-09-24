#include "phlex/core/products_consumer.hpp"
#include <spdlog/spdlog.h>

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
      std::string error = fmt::format("Must specify layers in the product selectors for node {}:\n"
                                      "  (Only invalid selectors are listed)\n{}",
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
                                       tbb::flow::graph& graph,
                                       require_layers layers_required) :
    consumer{std::move(name), std::move(predicates)},
    graph_{graph},
    input_products_{std::move(input_products)},
    layers_{layers_from(input_products_)}
  {
    validate_layers(layers_required, input_products_, this->name());
  }

  products_consumer::~products_consumer() = default;

  std::size_t products_consumer::num_inputs() const { return input().size(); }

  tbb::flow::receiver<message>& products_consumer::port(product_selector const& input_product)
  {
    auto& next = port_for(input_product);

    // If input_product doesn't have a layer, it must be for a node that allows layer omission
    if (input_product.layer) {
      auto& layer_check = layer_checkers_.emplace_back(std::make_unique<layer_check_node_t>(
        graph_,
        tbb::flow::unlimited,
        [&layer = static_cast<experimental::identifier const&>(input_product.layer)](
          message const& msg, auto& output) {
          if (msg.store->layer_name() == layer) {
            std::get<0>(output).try_put(msg);
          }
        }));
      make_edge(tbb::flow::output_port<0>(*layer_check), next);
      return *layer_check;
    }
    // else
    return next;
  }

  product_selectors const& products_consumer::input() const noexcept { return input_products_; }
  std::vector<phlex::experimental::identifier> const& products_consumer::layers() const noexcept
  {
    return layers_;
  }
}
