#ifndef phlex_core_double_bound_function_hpp
#define phlex_core_double_bound_function_hpp

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/core/node_options.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <concepts>
#include <functional>
#include <memory>

namespace phlex::experimental {

  template <typename Object, typename Predicate, typename Unfold>
  class double_bound_function :
    public node_options<double_bound_function<Object, Predicate, Unfold>> {
    using node_options_t = node_options<double_bound_function<Object, Predicate, Unfold>>;
    using input_parameter_types = constructor_parameter_types<Object>;

    // FIXME: Should maybe use some type of static assert, but not in a way that
    //        constrains the arguments of the Predicate and the Unfold to be the same.
    //
    // static_assert(
    //   std::same_as<function_parameter_types<Predicate>, function_parameter_types<Unfold>>);

  public:
    static constexpr auto N = std::tuple_size_v<input_parameter_types>;

    double_bound_function(configuration const* config,
                          std::string name,
                          Predicate predicate,
                          Unfold unfold,
                          concurrency c,
                          tbb::flow::graph& g,
                          node_catalog& nodes,
                          std::vector<std::string>& errors) :
      node_options_t{config},
      name_{detail::make_algorithm_name(config, std::move(name))},
      predicate_{std::move(predicate)},
      unfold_{std::move(unfold)},
      concurrency_{c.value},
      graph_{g},
      nodes_{nodes},
      errors_{errors}
    {
    }

    auto unfold(std::array<specified_label, N> input_args)
    {
      return partial_unfold<Object, Predicate, Unfold>{
        nodes_.registrar_for<declared_unfold_ptr>(errors_),
        std::move(name_),
        concurrency_,
        node_options_t::release_predicates(),
        graph_,
        std::move(predicate_),
        std::move(unfold_),
        std::vector(input_args.begin(), input_args.end())};
    }

    auto unfold(label_compatible auto... input_args)
    {
      static_assert(N == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return unfold({specified_label{std::forward<decltype(input_args)>(input_args)}...});
    }

  private:
    algorithm_name name_;
    Predicate predicate_;
    Unfold unfold_;
    std::size_t concurrency_;
    tbb::flow::graph& graph_;
    node_catalog& nodes_;
    std::vector<std::string>& errors_;
  };
}

#endif // phlex_core_double_bound_function_hpp
