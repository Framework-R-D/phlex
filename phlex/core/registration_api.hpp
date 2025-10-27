#ifndef phlex_core_registration_api_hpp
#define phlex_core_registration_api_hpp

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/core/node_options.hpp"
#include "phlex/core/upstream_predicates.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <concepts>
#include <functional>
#include <memory>

namespace phlex::experimental {

  // ====================================================================================
  // Registration API

  template <template <typename...> typename HOF, typename AlgorithmBits>
  class registration_api {
    using hof_type = HOF<AlgorithmBits>;
    using NodePtr = typename hof_type::node_ptr_type;

    static constexpr auto N = AlgorithmBits::number_inputs;
    static constexpr auto M = hof_type::number_output_products;

  public:
    registration_api(configuration const* config,
                     std::string name,
                     AlgorithmBits alg,
                     concurrency c,
                     tbb::flow::graph& g,
                     node_catalog& nodes,
                     std::vector<std::string>& errors) :
      config_{config},
      name_{detail::make_algorithm_name(config, std::move(name))},
      alg_{std::move(alg)},
      concurrency_{c},
      graph_{g},
      registrar_{nodes.registrar_for<NodePtr>(errors)}
    {
    }

    auto input_family(std::array<specified_label, N> input_args)
    {
      registrar_.set_creator([this, inputs = std::move(input_args)](auto predicates) {
        return std::make_unique<hof_type>(std::move(name_),
                                          concurrency_.value,
                                          std::move(predicates),
                                          graph_,
                                          std::move(alg_),
                                          std::vector(inputs.begin(), inputs.end()));
      });
      return upstream_predicates<NodePtr>{std::move(registrar_), config_};
    }

    template <label_compatible L>
    auto input_family(std::array<L, N> input_args)
    {
      return input_family(to_labels(input_args));
    }

    auto input_family(label_compatible auto... input_args)
    {
      static_assert(N == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return input_family(
        {specified_label::create(std::forward<decltype(input_args)>(input_args))...});
    }

  private:
    configuration const* config_;
    algorithm_name name_;
    AlgorithmBits alg_;
    concurrency concurrency_;
    tbb::flow::graph& graph_;
    registrar<NodePtr> registrar_;
  };

  template <template <typename...> typename HOF, typename AlgorithmBits>
  auto make_registration(configuration const* config,
                         std::string name,
                         AlgorithmBits alg,
                         concurrency c,
                         tbb::flow::graph& g,
                         node_catalog& nodes,
                         std::vector<std::string>& errors)
  {
    return registration_api<HOF, AlgorithmBits>{
      config, std::move(name), std::move(alg), c, g, nodes, errors};
  }

  template <typename T, typename FT>
  class bound_function : public node_options<bound_function<T, FT>> {
    using node_options_t = node_options<bound_function<T, FT>>;
    using input_parameter_types = function_parameter_types<FT>;

    static constexpr auto N = number_parameters<FT>;

  public:
    bound_function(configuration const* config,
                   std::string name,
                   std::shared_ptr<T> obj,
                   FT f,
                   concurrency c,
                   tbb::flow::graph& g,
                   node_catalog& nodes,
                   std::vector<std::string>& errors) :
      node_options_t{config},
      name_{detail::make_algorithm_name(config, std::move(name))},
      obj_{obj},
      ft_{std::move(f)},
      concurrency_{c},
      graph_{g},
      nodes_{nodes},
      errors_{errors}
    {
    }

    auto transform(std::array<specified_label, N> input_args)
      requires is_transform_like<FT>
    {
      return pre_transform{nodes_.registrar_for<declared_transform_ptr>(errors_),
                           std::move(name_),
                           concurrency_.value,
                           node_options_t::release_predicates(),
                           graph_,
                           algorithm_bits(obj_, ft_),
                           std::vector(input_args.begin(), input_args.end())};
    }

    auto fold(std::array<specified_label, N - 1> input_args)
      requires is_fold_like<FT>
    {
      return pre_fold{nodes_.registrar_for<declared_fold_ptr>(errors_),
                      std::move(name_),
                      concurrency_.value,
                      node_options_t::release_predicates(),
                      graph_,
                      algorithm_bits(obj_, ft_),
                      std::vector(input_args.begin(), input_args.end())};
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

#endif // phlex_core_registration_api_hpp
