#include "phlex/core/consumer.hpp"

#include "phlex/core/join_registration.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <fmt/format.h>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

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
    input_products_{std::move(input_products)},
    layers_{layers_from(input_products_)},
    accepted_cells_(input_products_.size())
  {
    validate_layers(layers_required, input_products_, this->name());
  }

  consumer::~consumer() = default;

  phlex::experimental::algorithm_name const& consumer::name() const noexcept { return name_; }

  std::vector<std::string> const& consumer::when() const noexcept { return predicates_; }

  tbb::flow::receiver<message>& consumer::port(product_selector const& input_product)
  {
    auto& next = port_for(input_product);

    // If input_product doesn't have a layer, it must be for a node that allows layer omission
    if (input_product.layer) {
      auto const input_ordinal = port_index_for(input_products_, input_product);
      auto& layer_check = layer_checkers_.emplace_back(std::make_unique<layer_check_node_t>(
        graph_, tbb::flow::unlimited, [this, input_ordinal](message const& msg, auto& output) {
          auto const& accepted = accepted_cells_[input_ordinal];
          if (msg.store->layer_name() == layers_[input_ordinal] and
              (not accepted or accepted->contains(msg.store->index()->layer_hash()))) {
            std::get<0>(output).try_put(msg);
          }
        }));
      make_edge(tbb::flow::output_port<0>(*layer_check), next);
      return *layer_check;
    }
    // else
    return next;
  }

  join_registration consumer::index_ports()
  {
    auto result = index_ports_for();
    for (auto& participant : result.participants) {
      if (participant.input_ordinal) {
        participant.accept_cells = [this, input_ordinal = *participant.input_ordinal](
                                     std::unordered_set<std::size_t> cells) {
          accepted_cells_[input_ordinal] = std::move(cells);
        };
      }
    }
    return result;
  }

  product_selectors const& consumer::input() const noexcept { return input_products_; }
  std::vector<phlex::experimental::identifier> const& consumer::layers() const noexcept
  {
    return layers_;
  }
}
