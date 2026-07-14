#include "phlex/core/provider_node.hpp"
#include "phlex/model/product_store.hpp"

#include "spdlog/spdlog.h"

#include <functional>
#include <memory>
#include <utility>

namespace phlex::detail {
  provider_node::provider_node(tbb::flow::graph& g, provider_bundle bundle) :
    provider_node{bundle.spec.creator(),
                  bundle.max_concurrency.value,
                  g,
                  std::move(bundle.provider_function),
                  bundle.spec, // copied to avoid potential error reading from moved-from bundle
                  phlex::experimental::identifier{bundle.layer},
                  phlex::experimental::identifier{bundle.stage}}
  {
  }

  provider_node::provider_node(phlex::experimental::algorithm_name algo_name,
                               std::size_t concurrency,
                               tbb::flow::graph& g,
                               provider_function provider_func,
                               product_specification output_spec,
                               phlex::experimental::identifier output_layer,
                               phlex::experimental::identifier stage) :
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

                // The constructor argument 1uz specifies how many slots to reserve in the
                // underlying product container.  For providers, only one data product is
                // produced per input index.
                products new_products{1uz};
                new_products.add(output_, std::move(new_product));
                auto store = std::make_shared<phlex::experimental::product_store>(
                  index, name_, std::move(new_products), stage_);

                return {.store = std::move(store), .id = msg_id};
              }}
  {
    spdlog::debug("Created provider node {} making output {} ϵ {}",
                  name().to_string(),
                  output_.to_string(),
                  layer_);
  }

  phlex::experimental::algorithm_name const& provider_node::name() const noexcept { return name_; }

  product_specification const& provider_node::output_product() const noexcept { return output_; }

  phlex::experimental::identifier const& provider_node::layer() const noexcept { return layer_; }

  phlex::experimental::identifier const& provider_node::stage() const noexcept { return stage_; }

}
