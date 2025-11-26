#ifndef PHLEX_CORE_DECLARED_PROVIDER_HPP
#define PHLEX_CORE_DECLARED_PROVIDER_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_unordered_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <utility>

namespace phlex::experimental {

  class declared_provider : public products_consumer {
  public:
    declared_provider(algorithm_name name, product_queries output_products);
    virtual ~declared_provider();

    virtual tbb::flow::sender<message>& sender() = 0;
    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual product_specifications const& output() const = 0;
    virtual std::size_t num_calls() const = 0;
  };

  using declared_provider_ptr = std::unique_ptr<declared_provider>;
  using declared_providers = simple_ptr_map<declared_provider_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class provider_node : public declared_provider {
    using function_t = typename AlgorithmBits::bound_type;
    static constexpr auto M = number_output_objects<function_t>;

  public:
    using node_ptr_type = declared_provider_ptr;
    // We are setting number_output_products to 0 because that will allow the
    // registration mechanism to work, for now.
    static constexpr auto number_output_products = 0;

    provider_node(algorithm_name name,
                  std::size_t concurrency,
                  std::vector<std::string> /*ignored */,
                  tbb::flow::graph& g,
                  AlgorithmBits alg,
                  product_queries output) :
      // Fixme: get rid of copying of output.
      declared_provider{std::move(name), output},
      output_{output[0].name},
      provider_{
        g, concurrency, [this, ft = alg.release_algorithm()](message const& msg, auto& output) {
          auto& [stay_in_graph, to_output] = output;

          auto result = std::invoke(ft, *msg.store->id());
          ++calls_;

          products new_products;
          // Add all adds all products; we should only have one. Fix this later.
          new_products.add_all(output_, std::move(result));
          auto store = msg.store->make_continuation(this->full_name(), std::move(new_products));

          message const new_msg{store, msg.eom, msg.id};
          stay_in_graph.try_put(new_msg);
          to_output.try_put(new_msg);
        }}
    {
    }

    tbb::flow::receiver<message>& receiver() { return provider_; }

    tbb::flow::receiver<message>& port_for(product_query const&) override { return provider_; }
    std::vector<tbb::flow::receiver<message>*> ports() override { return {&provider_}; }

  private:
    tbb::flow::sender<message>& sender() override { return output_port<0>(provider_); }
    tbb::flow::sender<message>& to_output() override { return output_port<1>(provider_); }
    product_specifications const& output() const override { return output_; }

    std::size_t num_calls() const final { return calls_.load(); }
    product_specifications output_;
    tbb::flow::multifunction_node<message, messages_t<2u>> provider_;
    std::atomic<std::size_t> calls_;
  };

}

#endif // PHLEX_CORE_DECLARED_PROVIDER_HPP
