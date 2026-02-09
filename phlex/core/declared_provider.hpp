#ifndef PHLEX_CORE_DECLARED_PROVIDER_HPP
#define PHLEX_CORE_DECLARED_PROVIDER_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/store_counters.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_unordered_map.h"
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

    virtual tbb::flow::receiver<message>* input_port() = 0;
    virtual tbb::flow::sender<message>& sender() = 0;
    virtual std::size_t num_calls() const = 0;

  protected:
    using stores_t = tbb::concurrent_hash_map<data_cell_index::hash_type, product_store_ptr>;
    using const_accessor = stores_t::const_accessor;

    void report_cached_stores(stores_t const& stores) const;

  private:
    algorithm_name name_;
    product_query output_product_;
  };

  using declared_provider_ptr = std::unique_ptr<declared_provider>;
  using declared_providers = simple_ptr_map<declared_provider_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class provider_node : public declared_provider, private detect_flush_flag {
    using function_t = typename AlgorithmBits::bound_type;

  public:
    using node_ptr_type = declared_provider_ptr;

    provider_node(algorithm_name name,
                  std::size_t concurrency,
                  tbb::flow::graph& g,
                  AlgorithmBits alg,
                  product_query output) :
      declared_provider{std::move(name), output},
      output_{output.spec()},
      provider_{
        g, concurrency, [this, ft = alg.release_algorithm()](message const& msg, auto& output) {
          auto& [stay_in_graph, to_output] = output;

          if (msg.store->is_flush()) {
            flag_for(msg.store->index()->hash()).flush_received(msg.original_id);
            stay_in_graph.try_put(msg);
          } else {
            // Check cache first
            auto index_hash = msg.store->index()->hash();
            if (const_accessor ca; cache_.find(ca, index_hash)) {
              // Cache hit - reuse the cached store
              message const new_msg{ca->second, msg.eom, msg.id};
              stay_in_graph.try_put(new_msg);
              to_output.try_put(new_msg);
              return;
            }

            // Cache miss - compute the result
            auto result = std::invoke(ft, *msg.store->index());
            ++calls_;

            products new_products;
            new_products.add(output_.name(), std::move(result));
            auto store = std::make_shared<product_store>(
              msg.store->index(), this->full_name(), std::move(new_products));

            // Store in cache
            cache_.emplace(index_hash, store);

            message const new_msg{store, msg.eom, msg.id};
            stay_in_graph.try_put(new_msg);
            to_output.try_put(new_msg);
            flag_for(msg.store->index()->hash()).mark_as_processed();
          }

          if (done_with(msg.store)) {
            cache_.erase(msg.store->index()->hash());
          }
        }}
    {
      spdlog::debug(
        "Created provider node {} making output {}", this->full_name(), output.to_string());
    }

    ~provider_node() { report_cached_stores(cache_); }

  private:
    tbb::flow::receiver<message>* input_port() override { return &provider_; }
    tbb::flow::sender<message>& sender() override { return output_port<0>(provider_); }

    std::size_t num_calls() const final { return calls_.load(); }

    product_specification output_;
    tbb::flow::multifunction_node<message, messages_t<2u>> provider_;
    std::atomic<std::size_t> calls_;
    stores_t cache_;
  };

}

#endif // PHLEX_CORE_DECLARED_PROVIDER_HPP
