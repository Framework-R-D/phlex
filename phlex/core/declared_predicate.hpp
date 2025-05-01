#ifndef phlex_core_declared_predicate_hpp
#define phlex_core_declared_predicate_hpp

#include "phlex/core/concepts.hpp"
#include "phlex/core/detail/filter_impl.hpp"
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
#include "phlex/utilities/sized_tuple.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/concurrent_unordered_map.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

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
#include <tuple>
#include <type_traits>
#include <utility>

namespace phlex::experimental {

  class declared_predicate : public products_consumer {
  public:
    declared_predicate(algorithm_name name, std::vector<std::string> predicates);
    virtual ~declared_predicate();

    virtual tbb::flow::sender<predicate_result>& sender() = 0;
  };

  using declared_predicate_ptr = std::unique_ptr<declared_predicate>;
  using declared_predicates = std::map<std::string, declared_predicate_ptr>;

  // =====================================================================================

  template <is_predicate_like FT, typename InputArgs>
  class pre_predicate {
    static constexpr std::size_t N = std::tuple_size_v<InputArgs>;
    using function_t = FT;

    class complete_predicate;

  public:
    pre_predicate(registrar<declared_predicates> reg,
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
      reg_.set([this] { return create(); });
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

  private:
    declared_predicate_ptr create()
    {
      return std::make_unique<complete_predicate>(std::move(name_),
                                                  concurrency_,
                                                  std::move(predicates_),
                                                  graph_,
                                                  std::move(ft_),
                                                  std::move(input_args_),
                                                  std::move(product_labels_));
    }
    algorithm_name name_;
    std::size_t concurrency_;
    std::vector<std::string> predicates_;
    tbb::flow::graph& graph_;
    function_t ft_;
    InputArgs input_args_;
    std::array<specified_label, N> product_labels_;
    registrar<declared_predicates> reg_;
  };

  // =====================================================================================

  template <is_predicate_like FT, typename InputArgs>
  class pre_predicate<FT, InputArgs>::complete_predicate :
    public declared_predicate,
    private detect_flush_flag {
    static constexpr auto N = std::tuple_size_v<InputArgs>;
    using function_t = FT;
    using results_t = tbb::concurrent_hash_map<level_id::hash_type, predicate_result>;
    using accessor = results_t::accessor;
    using const_accessor = results_t::const_accessor;

  public:
    complete_predicate(algorithm_name name,
                       std::size_t concurrency,
                       std::vector<std::string> predicates,
                       tbb::flow::graph& g,
                       function_t&& f,
                       InputArgs input,
                       std::array<specified_label, N> product_labels) :
      declared_predicate{std::move(name), std::move(predicates)},
      product_labels_{std::move(product_labels)},
      input_{std::move(input)},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      predicate_{g,
                 concurrency,
                 [this, ft = std::move(f)](messages_t<N> const& messages) -> predicate_result {
                   auto const& msg = most_derived(messages);
                   auto const& [store, message_id] = std::tie(msg.store, msg.id);
                   predicate_result result{};
                   if (store->is_flush()) {
                     flag_for(store->id()->hash()).flush_received(message_id);
                   } else if (const_accessor a; results_.find(a, store->id()->hash())) {
                     result = {msg.eom, message_id, a->second.result};
                   } else if (accessor a; results_.insert(a, store->id()->hash())) {
                     bool const rc = call(ft, messages, std::make_index_sequence<N>{});
                     result = a->second = {msg.eom, message_id, rc};
                     flag_for(store->id()->hash()).mark_as_processed();
                   }

                   if (done_with(store)) {
                     results_.erase(store->id()->hash());
                   }
                   return result;
                 }}
    {
      make_edge(join_, predicate_);
    }

    ~complete_predicate()
    {
      if (results_.size() > 0ull) {
        spdlog::warn("Filter {} has {} cached results.", full_name(), results_.size());
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(specified_label const& product_label) override
    {
      return receiver_for<N>(join_, product_labels_, product_label);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    tbb::flow::sender<predicate_result>& sender() override { return predicate_; }
    specified_labels input() const override { return product_labels_; }

    template <std::size_t... Is>
    bool call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      ++calls_;
      return std::invoke(ft, std::get<Is>(input_).retrieve(messages)...);
    }

    std::size_t num_calls() const final { return calls_.load(); }

    std::array<specified_label, N> product_labels_;
    InputArgs input_;
    join_or_none_t<N> join_;
    tbb::flow::function_node<messages_t<N>, predicate_result> predicate_;
    results_t results_;
    std::atomic<std::size_t> calls_;
  };

}

#endif // phlex_core_declared_predicate_hpp
