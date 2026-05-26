#include "phlex/core/provider_node.hpp"
#include "phlex/model/product_store.hpp"

#include "spdlog/spdlog.h"

#include <functional>
#include <memory>
#include <utility>

namespace phlex::experimental {
  provider_node::provider_node(algorithm_name algo_name,
                               std::size_t concurrency,
                               tbb::flow::graph& g,
                               provider_function provider_func,
                               product_specification output_spec,
                               identifier output_layer,
                               identifier stage) :
    name_{std::move(algo_name)},
    output_{std::move(output_spec)},
    layer_{std::move(output_layer)},
    stage_{std::move(stage)},
    provider_{g,
              concurrency,
              [this, ft = std::move(provider_func)](index_message const& index_msg) -> message {
                auto const [index, msg_id, _] = index_msg;

                auto new_product = std::invoke(ft, *index);
                ++calls_;

                products new_products{1uz};
                new_products.add(output_, std::move(new_product));
                auto store = std::make_shared<product_store>(index, name_, std::move(new_products));

                return {.store = std::move(store), .id = msg_id};
              }}
  {
    spdlog::debug(
      "Created provider node {} making output {} ϵ {}", name().full(), output_.full(), layer_);
  }

  algorithm_name const& provider_node::name() const noexcept { return name_; }

  product_specification const& provider_node::output_product() const noexcept { return output_; }

  identifier const& provider_node::layer() const noexcept { return layer_; }

  identifier const& provider_node::stage() const noexcept { return stage_; }

}
