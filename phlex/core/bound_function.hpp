#ifndef phlex_core_bound_function_hpp
#define phlex_core_bound_function_hpp

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/core/node_options.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <concepts>
#include <functional>
#include <memory>

namespace phlex::experimental {

  template <typename T, typename FT>
  class bound_function : public node_options<bound_function<T, FT>> {
    using node_options_t = node_options<bound_function<T, FT>>;
    using input_parameter_types = function_parameter_types<FT>;

  public:
    static constexpr auto N = number_parameters<FT>;

    bound_function(configuration const* config,
                   std::string name,
                   std::shared_ptr<T> obj,
                   FT f,
                   concurrency c,
                   tbb::flow::graph& g,
                   node_catalog& nodes,
                   std::vector<std::string>& errors) :
      node_options_t{config},
      name_{config ? config->get<std::string>("module_label") : "", std::move(name)},
      obj_{obj},
      ft_{std::move(f)},
      concurrency_{c},
      graph_{g},
      nodes_{nodes},
      errors_{errors}
    {
    }

    auto evaluate(std::array<specified_label, N> input_args)
      requires is_predicate_like<FT>
    {
      auto inputs =
        form_input_arguments<input_parameter_types>(name_.full(), std::move(input_args));
      return pre_predicate{nodes_.register_predicate(errors_),
                           std::move(name_),
                           concurrency_.value,
                           node_options_t::release_predicates(),
                           graph_,
                           delegate(obj_, ft_),
                           std::move(inputs)};
    }

    auto observe(std::array<specified_label, N> input_args)
      requires is_observer_like<FT>
    {
      auto inputs =
        form_input_arguments<input_parameter_types>(name_.full(), std::move(input_args));
      return pre_observer{nodes_.register_observer(errors_),
                          std::move(name_),
                          concurrency_.value,
                          node_options_t::release_predicates(),
                          graph_,
                          delegate(obj_, ft_),
                          std::move(inputs)};
    }

    auto transform(std::array<specified_label, N> input_args)
      requires is_transform_like<FT>
    {
      auto inputs =
        form_input_arguments<input_parameter_types>(name_.full(), std::move(input_args));
      return pre_transform{nodes_.register_transform(errors_),
                           std::move(name_),
                           concurrency_.value,
                           node_options_t::release_predicates(),
                           graph_,
                           delegate(obj_, ft_),
                           std::move(inputs)};
    }

    auto fold(std::array<specified_label, N - 1> input_args)
      requires is_fold_like<FT>
    {
      using all_but_first = skip_first_type<input_parameter_types>;
      auto inputs = form_input_arguments<all_but_first>(name_.full(), std::move(input_args));
      return pre_fold{nodes_.register_fold(errors_),
                      std::move(name_),
                      concurrency_.value,
                      node_options_t::release_predicates(),
                      graph_,
                      delegate(obj_, ft_),
                      std::move(inputs)};
    }

    template <label_compatible L>
    auto evaluate(std::array<L, N> input_args)
    {
      return evaluate(to_labels(input_args));
    }

    template <label_compatible L>
    auto observe(std::array<L, N> input_args)
    {
      return observe(to_labels(input_args));
    }

    template <label_compatible L>
    auto transform(std::array<L, N> input_args)
    {
      return transform(to_labels(input_args));
    }

    template <label_compatible L>
    auto fold(std::array<L, N> input_args)
    {
      return fold(to_labels(input_args));
    }

    auto evaluate(label_compatible auto... input_args)
    {
      static_assert(N == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return evaluate({specified_label::create(std::forward<decltype(input_args)>(input_args))...});
    }

    auto observe(label_compatible auto... input_args)
    {
      static_assert(N == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return observe({specified_label::create(std::forward<decltype(input_args)>(input_args))...});
    }

    auto transform(label_compatible auto... input_args)
    {
      static_assert(N == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return transform(
        {specified_label::create(std::forward<decltype(input_args)>(input_args))...});
    }

    auto fold(label_compatible auto... input_args)
    {
      static_assert(N - 1 == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return fold({specified_label::create(std::forward<decltype(input_args)>(input_args))...});
    }

  private:
    algorithm_name name_;
    std::shared_ptr<T> obj_;
    FT ft_;
    concurrency concurrency_;
    tbb::flow::graph& graph_;
    node_catalog& nodes_;
    std::vector<std::string>& errors_;
  };

  template <typename T, typename FT>
  bound_function(configuration const*, std::string, std::shared_ptr<T>, FT, node_catalog&)
    -> bound_function<T, FT>;
}

#endif // phlex_core_bound_function_hpp
