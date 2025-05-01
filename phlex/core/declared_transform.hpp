#ifndef phlex_core_declared_transform_hpp
#define phlex_core_declared_transform_hpp

// FIXME: Add comments explaining the process.  For each implementation, explain what part
//        of the process a given section of code is addressing.

#include "phlex/core/concepts.hpp"
#include "phlex/core/detail/port_names.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/core/store_counters.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/qualified_name.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_unordered_map.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace phlex::experimental {

  class declared_transform : public products_consumer {
  public:
    declared_transform(algorithm_name name, std::vector<std::string> predicates);
    virtual ~declared_transform();

    virtual tbb::flow::sender<message>& sender() = 0;
    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual qualified_names output() const = 0;
    virtual std::size_t product_count() const = 0;
  };

  using declared_transform_ptr = std::unique_ptr<declared_transform>;
  using declared_transforms = std::map<std::string, declared_transform_ptr>;

  // =====================================================================================

  template <is_transform_like FT, typename InputArgs>
  class pre_transform {
    static constexpr std::size_t N = std::tuple_size_v<InputArgs>;
    static constexpr std::size_t M = number_output_objects<FT>;
    using function_t = FT;

    template <std::size_t M>
    class total_transform;

  public:
    pre_transform(registrar<declared_transforms> reg,
                  algorithm_name name,
                  std::size_t concurrency,
                  std::vector<std::string> predicates,
                  tbb::flow::graph& g,
                  function_t&& f,
                  InputArgs input_args) :
      name_{std::move(name)},
      concurrency_{concurrency},
      predicates_{std::move(predicates)},
      graph_{g},
      ft_{std::move(f)},
      input_args_{std::move(input_args)},
      product_labels_{detail::port_names(input_args_)},
      reg_{std::move(reg)}
    {
    }

    auto& for_each(std::string const& family)
    {
      for (auto& allowed_family : product_labels_ | std::views::transform(to_family)) {
        if (empty(allowed_family)) {
          allowed_family = family;
        }
      }
      return *this;
    }

    template <std::size_t Msize>
    auto& to(std::array<std::string, Msize> output_keys)
    {
      static_assert(
        M == Msize,
        "The number of function parameters is not the same as the number of returned output "
        "objects.");

      std::array<qualified_name, Msize> outputs;
      std::ranges::transform(output_keys, outputs.begin(), to_qualified_name{name_});
      reg_.set([this, out = std::move(outputs)] { return create(std::move(out)); });
      return *this;
    }

    auto& to(std::convertible_to<std::string> auto&&... ts)
    {
      static_assert(
        M == sizeof...(ts),
        "The number of function parameters is not the same as the number of returned output "
        "objects.");
      return to(std::array<std::string, M>{std::forward<decltype(ts)>(ts)...});
    }

  private:
    declared_transform_ptr create(std::array<qualified_name, M> outputs)
    {
      return std::make_unique<total_transform<M>>(std::move(name_),
                                                  concurrency_,
                                                  std::move(predicates_),
                                                  graph_,
                                                  std::move(ft_),
                                                  std::move(input_args_),
                                                  std::move(product_labels_),
                                                  std::move(outputs));
    }

    algorithm_name name_;
    std::size_t concurrency_;
    std::vector<std::string> predicates_;
    tbb::flow::graph& graph_;
    function_t ft_;
    InputArgs input_args_;
    std::array<specified_label, N> product_labels_;
    registrar<declared_transforms> reg_;
  };

  // =====================================================================================

  template <is_transform_like FT, typename InputArgs>
  template <std::size_t M>
  class pre_transform<FT, InputArgs>::total_transform :
    public declared_transform,
    private detect_flush_flag {
    using stores_t = tbb::concurrent_hash_map<level_id::hash_type, product_store_ptr>;
    using accessor = stores_t::accessor;
    using const_accessor = stores_t::const_accessor;

  public:
    total_transform(algorithm_name name,
                    std::size_t concurrency,
                    std::vector<std::string> predicates,
                    tbb::flow::graph& g,
                    function_t&& f,
                    InputArgs input,
                    std::array<specified_label, N> product_labels,
                    std::array<qualified_name, M> output) :
      declared_transform{std::move(name), std::move(predicates)},
      product_labels_{std::move(product_labels)},
      input_{std::move(input)},
      output_{std::move(output)},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      transform_{
        g, concurrency, [this, ft = std::move(f)](messages_t<N> const& messages, auto& output) {
          auto const& msg = most_derived(messages);
          auto const& [store, message_eom, message_id] = std::tie(msg.store, msg.eom, msg.id);
          auto& [stay_in_graph, to_output] = output;
          if (store->is_flush()) {
            flag_for(store->id()->hash()).flush_received(msg.original_id);
            stay_in_graph.try_put(msg);
            to_output.try_put(msg);
          } else {
            accessor a;
            if (stores_.insert(a, store->id()->hash())) {
              auto result = call(ft, messages, std::make_index_sequence<N>{});
              ++calls_;
              ++product_count_[store->id()->level_hash()];
              products new_products;
              new_products.add_all(output_, std::move(result));
              a->second = store->make_continuation(this->full_name(), std::move(new_products));

              message const new_msg{a->second, msg.eom, message_id};
              stay_in_graph.try_put(new_msg);
              to_output.try_put(new_msg);
              flag_for(store->id()->hash()).mark_as_processed();
            } else {
              stay_in_graph.try_put({a->second, msg.eom, message_id});
            }
          }

          if (done_with(store)) {
            stores_.erase(store->id()->hash());
          }
        }}
    {
      make_edge(join_, transform_);
    }

    ~total_transform()
    {
      if (stores_.size() > 0ull) {
        spdlog::warn("Transform {} has {} cached stores.", full_name(), stores_.size());
      }
      for (auto const& [hash, store] : stores_) {
        spdlog::debug(" => ID: {} (hash: {})", store->id()->to_string(), hash);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(specified_label const& product_label) override
    {
      return receiver_for<N>(join_, product_labels_, product_label);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    tbb::flow::sender<message>& sender() override { return output_port<0>(transform_); }
    tbb::flow::sender<message>& to_output() override { return output_port<1>(transform_); }
    specified_labels input() const override { return product_labels_; }
    qualified_names output() const override { return output_; }

    template <std::size_t... Is>
    auto call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      return std::invoke(ft, std::get<Is>(input_).retrieve(messages)...);
    }

    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final
    {
      std::size_t result{};
      for (auto const& count : product_count_ | std::views::values) {
        result += count.load();
      }
      return result;
    }

    std::array<specified_label, N> product_labels_;
    InputArgs input_;
    std::array<qualified_name, M> output_;
    join_or_none_t<N> join_;
    tbb::flow::multifunction_node<messages_t<N>, messages_t<2u>> transform_;
    stores_t stores_;
    std::atomic<std::size_t> calls_;
    tbb::concurrent_unordered_map<std::size_t, std::atomic<std::size_t>> product_count_;
  };

}

#endif // phlex_core_declared_transform_hpp
