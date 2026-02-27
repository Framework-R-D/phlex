#ifndef PHLEX_CORE_DECLARED_PROVIDER_HPP
#define PHLEX_CORE_DECLARED_PROVIDER_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <utility>

namespace phlex::experimental {

  class declared_provider {
  public:
    declared_provider(algorithm_name name, product_query output_product);
    virtual ~declared_provider();

    std::string full_name() const;
    product_query const& output_product() const noexcept;
    identifier const& layer() const noexcept;

    virtual tbb::flow::receiver<index_message>* input_port() = 0;
    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual std::size_t num_calls() const = 0;

  private:
    algorithm_name name_;
    product_query output_product_;
  };

  using declared_provider_ptr = std::unique_ptr<declared_provider>;
  using declared_providers = simple_ptr_map<declared_provider_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class provider_node : public declared_provider {
  public:
    using node_ptr_type = declared_provider_ptr;

    provider_node(algorithm_name name,
                  std::size_t concurrency,
                  tbb::flow::graph& g,
                  AlgorithmBits alg,
                  product_query output) :
      declared_provider{std::move(name), output},
      output_{output.spec()},
      provider_{g,
                concurrency,
                [this, ft = alg.release_algorithm()](index_message const& index_msg, auto& output) {
                  auto const [index, msg_id, _] = index_msg;

                  auto result = std::invoke(ft, *index);
                  ++calls_;

                  products new_products;
                  new_products.add(output_.name(), std::move(result));
                  auto store = std::make_shared<product_store>(
                    index, this->full_name(), std::move(new_products));

                  std::get<0>(output).try_put({.store = std::move(store), .id = msg_id});
                }}
    {
      spdlog::debug(
        "Created provider node {} making output {}", this->full_name(), output.to_string());
    }

  private:
    tbb::flow::receiver<index_message>* input_port() override { return &provider_; }
    tbb::flow::sender<message>& output_port() override
    {
      return tbb::flow::output_port<0>(provider_);
    }

    std::size_t num_calls() const final { return calls_.load(); }

    product_specification output_;
    tbb::flow::multifunction_node<index_message, message_tuple<1u>> provider_;
    std::atomic<std::size_t> calls_;
  };

}

#endif // PHLEX_CORE_DECLARED_PROVIDER_HPP
