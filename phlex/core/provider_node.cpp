#include "phlex/core/provider_node.hpp"
#include "phlex/model/product_store.hpp"

#include "spdlog/spdlog.h"

#include <functional>
#include <memory>
#include <utility>

namespace phlex::experimental {
  provider_node::provider_node(algorithm_name name,
                               std::size_t concurrency,
                               tbb::flow::graph& g,
                               provider_function provider_func,
                               product_query output) :
    name_{std::move(name)},
    output_product_{output},
    output_{algorithm_name::create(std::string_view(identifier(output.creator))),
            output.suffix.value_or(identifier("")),
            output.type},
    provider_{g,
              concurrency,
              [this, ft = std::move(provider_func)](index_message const& index_msg) -> message {
                auto const [index, msg_id, _] = index_msg;

                auto new_product = std::invoke(ft, *index);
                ++calls_;

                products new_products;
                new_products.add(output_, std::move(new_product));
                auto store = std::make_shared<product_store>(
                  index, this->full_name(), std::move(new_products));

                return {.store = std::move(store), .id = msg_id};
              }}
  {
    spdlog::debug(
      "Created provider node {} making output {}", this->full_name(), output.to_string());
  }

  std::string provider_node::full_name() const { return name_.full(); }

  product_query const& provider_node::output_product() const noexcept { return output_product_; }

  identifier const& provider_node::layer() const noexcept { return output_product_.layer; }

}
