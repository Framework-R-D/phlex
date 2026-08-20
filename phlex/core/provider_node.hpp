#ifndef PHLEX_CORE_PROVIDER_NODE_HPP
#define PHLEX_CORE_PROVIDER_NODE_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {

  // Function type for type-erased data-product types (used by implicit providers)
  using provider_function = std::function<product_ptr(data_cell_index const&)>;

  struct PHLEX_CORE_EXPORT provider_bundle {
    phlex::detail::provider_function provider_function;
    concurrency max_concurrency;
    product_specification spec;
    std::string layer;
    std::string stage;
  };

  using provider_bundles = std::vector<provider_bundle>;

  class PHLEX_CORE_EXPORT provider_node {
  public:
    provider_node(tbb::flow::graph& g, provider_bundle bundle);

    provider_node(phlex::experimental::algorithm_name algo_name,
                  std::size_t concurrency,
                  tbb::flow::graph& g,
                  provider_function provider_func,
                  product_specification output_spec,
                  phlex::experimental::identifier output_layer,
                  phlex::experimental::identifier stage);

    phlex::experimental::algorithm_name const& name() const noexcept;
    product_specification const& output_product() const noexcept;
    phlex::experimental::identifier const& layer() const noexcept;
    phlex::experimental::identifier const& stage() const noexcept;

    tbb::flow::receiver<index_message>* input_port() { return &provider_; }
    tbb::flow::sender<message>& output_port() { return provider_; }
    std::size_t num_calls() const { return calls_.load(); }

  private:
    phlex::experimental::algorithm_name name_;
    product_specification output_;
    phlex::experimental::identifier layer_;
    phlex::experimental::identifier stage_;
    tbb::flow::function_node<index_message, message> provider_;
    std::atomic<std::size_t> calls_;
  };

  using provider_node_ptr = std::unique_ptr<provider_node>;
  using provider_nodes = simple_ptr_map<provider_node_ptr>;
}

#endif // PHLEX_CORE_PROVIDER_NODE_HPP
