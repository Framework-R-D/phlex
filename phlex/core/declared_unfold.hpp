#ifndef phlex_core_declared_unfold_hpp
#define phlex_core_declared_unfold_hpp

#include "phlex/core/concepts.hpp"
#include "phlex/core/detail/port_names.hpp"
#include "phlex/core/end_of_message.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multiplexer.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/store_counters.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/qualified_name.hpp"
#include "phlex/utilities/sized_tuple.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <array>
#include <atomic>
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
#include <vector>

namespace phlex::experimental {

  class generator {
  public:
    explicit generator(product_store_const_ptr const& parent,
                       std::string const& node_name,
                       std::string const& new_level_name);
    product_store_const_ptr flush_store() const;

    product_store_const_ptr make_child_for(std::size_t const level_number, products new_products)
    {
      return make_child(level_number, std::move(new_products));
    }

  private:
    product_store_const_ptr make_child(std::size_t i, products new_products);
    product_store_ptr parent_;
    std::string_view node_name_;
    std::string const& new_level_name_;
    std::map<level_id::hash_type, std::size_t> child_counts_;
  };

  class declared_unfold : public products_consumer {
  public:
    declared_unfold(algorithm_name name, std::vector<std::string> predicates);
    virtual ~declared_unfold();

    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual qualified_names output() const = 0;
    virtual void finalize(multiplexer::head_ports_t head_ports) = 0;
    virtual std::size_t product_count() const = 0;
    virtual multiplexer::head_ports_t const& downstream_ports() const = 0;
  };

  using declared_unfold_ptr = std::unique_ptr<declared_unfold>;
  using declared_unfolds = std::map<std::string, declared_unfold_ptr>;

  // =====================================================================================

  template <typename Object, typename Predicate, typename Unfold, typename InputArgs>
  class partial_unfold {
    static constexpr std::size_t N = std::tuple_size_v<InputArgs>;

    template <std::size_t M>
    class complete_unfold;

  public:
    partial_unfold(registrar<declared_unfolds> reg,
                   algorithm_name name,
                   std::size_t concurrency,
                   std::vector<std::string> predicates,
                   tbb::flow::graph& g,
                   Predicate&& predicate,
                   Unfold&& unfold,
                   InputArgs input_args) :
      name_{std::move(name)},
      concurrency_{concurrency},
      predicates_{std::move(predicates)},
      graph_{g},
      predicate_{std::move(predicate)},
      unfold_{std::move(unfold)},
      input_args_{std::move(input_args)},
      product_labels_{detail::port_names(input_args_)},
      reg_{std::move(reg)}
    {
    }

    template <std::size_t M>
    auto& into(std::array<std::string, M> output_products)
    {
      std::array<qualified_name, M> outputs;
      std::ranges::transform(output_products, outputs.begin(), to_qualified_name{name_});
      reg_.set([this, out = std::move(outputs)] { return create(std::move(out)); });
      return *this;
    }

    auto& into(std::convertible_to<std::string> auto&&... ts)
    {
      return into(std::array<std::string, sizeof...(ts)>{std::forward<decltype(ts)>(ts)...});
    }

    auto& within_family(std::string new_level_name)
    {
      new_level_name_ = std::move(new_level_name);
      return *this;
    }

  private:
    template <std::size_t M>
    declared_unfold_ptr create(std::array<qualified_name, M> outputs)
    {
      return std::make_unique<complete_unfold<M>>(std::move(name_),
                                                  concurrency_,
                                                  std::move(predicates_),
                                                  graph_,
                                                  std::move(predicate_),
                                                  std::move(unfold_),
                                                  std::move(input_args_),
                                                  std::move(product_labels_),
                                                  std::move(outputs),
                                                  std::move(new_level_name_));
    }

    algorithm_name name_;
    std::size_t concurrency_;
    std::vector<std::string> predicates_;
    tbb::flow::graph& graph_;
    Predicate predicate_;
    Unfold unfold_;
    InputArgs input_args_;
    std::array<specified_label, N> product_labels_;
    std::string new_level_name_;
    registrar<declared_unfolds> reg_;
  };

  // =====================================================================================

  template <typename Object, typename Predicate, typename Unfold, typename InputArgs>
  template <std::size_t M>
  class partial_unfold<Object, Predicate, Unfold, InputArgs>::complete_unfold :
    public declared_unfold,
    private detect_flush_flag {
    using stores_t = tbb::concurrent_hash_map<level_id::hash_type, product_store_ptr>;
    using accessor = stores_t::accessor;
    using const_accessor = stores_t::const_accessor;

  public:
    complete_unfold(algorithm_name name,
                    std::size_t concurrency,
                    std::vector<std::string> predicates,
                    tbb::flow::graph& g,
                    Predicate&& predicate,
                    Unfold&& unfold,
                    InputArgs input,
                    std::array<specified_label, N> product_labels,
                    std::array<qualified_name, M> output_products,
                    std::string new_level_name) :
      declared_unfold{std::move(name), std::move(predicates)},
      product_labels_{std::move(product_labels)},
      input_{std::move(input)},
      output_{std::move(output_products)},
      new_level_name_{std::move(new_level_name)},
      multiplexer_{g},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      unfold_{
        g,
        concurrency,
        [this, p = std::move(predicate), ufold = std::move(unfold)](
          messages_t<N> const& messages) -> tbb::flow::continue_msg {
          auto const& msg = most_derived(messages);
          auto const& store = msg.store;
          if (store->is_flush()) {
            flag_for(store->id()->hash()).flush_received(msg.id);
          } else if (accessor a; stores_.insert(a, store->id()->hash())) {
            std::size_t const original_message_id{msg_counter_};
            generator g{msg.store, this->full_name(), new_level_name_};
            call(p, ufold, msg.store->id(), g, msg.eom, messages, std::make_index_sequence<N>{});
            multiplexer_.try_put({g.flush_store(), msg.eom, ++msg_counter_, original_message_id});
            flag_for(store->id()->hash()).mark_as_processed();
          }

          if (done_with(store)) {
            stores_.erase(store->id()->hash());
          }
          return {};
        }},
      to_output_{g}
    {
      make_edge(join_, unfold_);
      make_edge(to_output_, multiplexer_);
    }

    ~complete_unfold()
    {
      if (stores_.size() > 0ull) {
        spdlog::warn("Unfold {} has {} cached stores.", full_name(), stores_.size());
      }
      for (auto const& store : stores_ | std::views::values) {
        spdlog::debug(" => ID: ", store->id()->to_string());
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(specified_label const& product_label) override
    {
      return receiver_for<N>(join_, product_labels_, product_label);
    }
    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    tbb::flow::sender<message>& to_output() override { return to_output_; }

    specified_labels input() const override { return product_labels_; }
    qualified_names output() const override { return output_; }

    void finalize(multiplexer::head_ports_t head_ports) override
    {
      multiplexer_.finalize(std::move(head_ports));
    }

    multiplexer::head_ports_t const& downstream_ports() const override
    {
      return multiplexer_.downstream_ports();
    }

    template <std::size_t... Is>
    void call(Predicate const& predicate,
              Unfold const& unfold,
              level_id_ptr const& unfolded_id,
              generator& g,
              end_of_message_ptr const& eom,
              messages_t<N> const& messages,
              std::index_sequence<Is...>)
    {
      ++calls_;
      Object obj(std::get<Is>(input_).retrieve(messages)...);
      std::size_t counter = 0;
      auto running_value = obj.initial_value();
      while (std::invoke(predicate, obj, running_value)) {
        products new_products;
        auto new_id = unfolded_id->make_child(counter, new_level_name_);
        if constexpr (requires { std::invoke(unfold, obj, running_value, *new_id); }) {
          auto [next_value, prods] = std::invoke(unfold, obj, running_value, *new_id);
          new_products.add_all(output_, std::move(prods));
          running_value = next_value;
        } else {
          auto [next_value, prods] = std::invoke(unfold, obj, running_value);
          new_products.add_all(output_, std::move(prods));
          running_value = next_value;
        }
        ++product_count_;
        auto child = g.make_child_for(counter++, std::move(new_products));
        to_output_.try_put({child, eom->make_child(child->id()), ++msg_counter_});
      }
    }

    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final { return product_count_.load(); }

    std::array<specified_label, N> product_labels_;
    InputArgs input_;
    std::array<qualified_name, M> output_;
    std::string new_level_name_;
    multiplexer multiplexer_;
    join_or_none_t<N> join_;
    tbb::flow::function_node<messages_t<N>> unfold_;
    tbb::flow::broadcast_node<message> to_output_;
    tbb::concurrent_hash_map<level_id::hash_type, product_store_ptr> stores_;
    std::atomic<std::size_t> msg_counter_{}; // Is this sufficient?  Probably not.
    std::atomic<std::size_t> calls_{};
    std::atomic<std::size_t> product_count_{};
  };
}

#endif // phlex_core_declared_unfold_hpp
