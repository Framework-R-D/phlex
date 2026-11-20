#include "phlex/core/consumer.hpp"

#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/layer_path.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <fmt/format.h>
#include <oneapi/tbb/flow_graph.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
  std::vector<phlex::experimental::layer_path> layers_from(phlex::product_selectors const& queries)
  {
    std::vector<phlex::experimental::layer_path> result;
    result.reserve(queries.size());
    for (auto const& query : queries) {
      if (query.layer) {
        result.push_back(*query.layer);
      }
    }
    return result;
  }

  void validate_layers(phlex::detail::require_layers layers_required,
                       phlex::product_selectors const& inputs,
                       phlex::experimental::algorithm_name const& algo)
  {
    using namespace phlex::detail;
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
      std::string const error =
        fmt::format("Must specify layers in the product selectors for node {}:\n"
                    "  (Only invalid selectors are listed)\n{}",
                    algo.to_string(),
                    bulleted_list(err_selectors));
      throw std::runtime_error(error);
    }
  }

  // Selectors on one algorithm whose layers are suffixes of one another name the same layer, so each
  // is lengthened to the longest of them. The hierarchy isn't known when an algorithm is registered,
  // so a short selector such as "event" is lengthened even when a deeper layer such as
  // "/job/run/event/event" also exists. Select the deeper layer with its own longer path to keep it.
  void normalize_layer_selectors(phlex::product_selectors& selectors)
  {
    if (selectors.size() <= 1) {
      return;
    }
    using phlex::experimental::layer_path;
    auto const layers = selectors | std::views::transform([](auto const& selector) {
                          return static_cast<layer_path const&>(selector.layer);
                        }) |
                        std::ranges::to<std::vector>();
    for (auto const& [selector, layer] : std::views::zip(selectors, layers)) {
      auto extensions =
        layers | std::views::filter([&layer](auto const& other) { return other.ends_with(layer); });
      selector.layer = *std::ranges::max_element(extensions, {}, &layer_path::depth);
    }
  }
}

namespace phlex::detail {
  consumer::consumer(phlex::experimental::algorithm_name name,
                     std::vector<std::string> predicates,
                     product_selectors input_products,
                     tbb::flow::graph& graph,
                     require_layers layers_required) :
    name_{std::move(name)},
    predicates_{std::move(predicates)},
    graph_{graph},
    input_products_{std::move(input_products)}
  {
    validate_layers(layers_required, input_products_, this->name());
    normalize_layer_selectors(input_products_);
    layers_ = layers_from(input_products_);
  }

  consumer::~consumer() = default;

  phlex::experimental::algorithm_name const& consumer::name() const noexcept { return name_; }

  std::vector<std::string> const& consumer::when() const noexcept { return predicates_; }

  tbb::flow::receiver<message>& consumer::port(product_selector const& input_product)
  {
    auto& next = port_for(input_product);

    // If input_product doesn't have a layer, it must be for a node that allows layer omission
    if (input_product.layer) {
      auto& layer_check = layer_checkers_.emplace_back(std::make_unique<layer_check_node_t>(
        graph_,
        tbb::flow::unlimited,
        [&layer = static_cast<experimental::layer_path const&>(input_product.layer)](
          message const& msg, auto& output) {
          if (msg.store->layer_path_ends_with(layer)) {
            std::get<0>(output).try_put(msg);
          }
        }));
      make_edge(tbb::flow::output_port<0>(*layer_check), next);
      return *layer_check;
    }
    // else
    return next;
  }

  product_selectors const& consumer::input() const noexcept { return input_products_; }
  std::vector<phlex::experimental::layer_path> const& consumer::layers() const noexcept
  {
    return layers_;
  }
}
