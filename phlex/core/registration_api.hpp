#ifndef phlex_core_registration_api_hpp
#define phlex_core_registration_api_hpp

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/declared_fold.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/core/upstream_predicates.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <concepts>
#include <functional>
#include <memory>

namespace phlex::experimental {
  class configuration;

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
      if constexpr (M == 0ull) {
        registrar_.set_creator(
          [this, inputs = std::move(input_args)](auto predicates, auto /* output_products */) {
            return std::make_unique<hof_type>(std::move(name_),
                                              concurrency_.value,
                                              std::move(predicates),
                                              graph_,
                                              std::move(alg_),
                                              std::vector(inputs.begin(), inputs.end()));
          });
      } else {
        registrar_.set_creator(
          [this, inputs = std::move(input_args)](auto predicates, auto output_products) {
            return std::make_unique<hof_type>(std::move(name_),
                                              concurrency_.value,
                                              std::move(predicates),
                                              graph_,
                                              std::move(alg_),
                                              std::vector(inputs.begin(), inputs.end()),
                                              std::move(output_products));
          });
      }
      return upstream_predicates<NodePtr, M>{std::move(registrar_), config_};
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

  // ====================================================================================
  // Fold API

  template <typename AlgorithmBits, typename... InitArgs>
  class fold_api {
    using InitTuple = std::tuple<InitArgs...>;

    static constexpr auto N = AlgorithmBits::number_inputs;
    static constexpr auto M = 1; // For now

  public:
    fold_api(configuration const* config,
             std::string name,
             AlgorithmBits alg,
             concurrency c,
             tbb::flow::graph& g,
             node_catalog& nodes,
             std::vector<std::string>& errors,
             std::string partition,
             InitArgs&&... init_args) :
      config_{config},
      name_{detail::make_algorithm_name(config, std::move(name))},
      alg_{std::move(alg)},
      concurrency_{c},
      graph_{g},
      partition_{std::move(partition)},
      init_{std::forward<InitArgs>(init_args)...},
      registrar_{nodes.registrar_for<declared_fold_ptr>(errors)}
    {
    }

    auto input_family(std::array<specified_label, N - 1> input_args)
    {
      registrar_.set_creator(
        [this, inputs = std::move(input_args)](auto predicates, auto output_products) {
          return std::make_unique<fold_node<AlgorithmBits, InitTuple>>(
            std::move(name_),
            concurrency_.value,
            std::move(predicates),
            graph_,
            std::move(alg_),
            std::move(init_),
            std::vector(inputs.begin(), inputs.end()),
            std::move(output_products),
            std::move(partition_));
        });
      return upstream_predicates<declared_fold_ptr, M>{std::move(registrar_), config_};
    }

    template <label_compatible L>
    auto input_family(std::array<L, N> input_args)
    {
      return input_family(to_labels(input_args));
    }

    auto input_family(label_compatible auto... input_args)
    {
      static_assert(N - 1 == sizeof...(input_args),
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
    std::string partition_;
    InitTuple init_;
    registrar<declared_fold_ptr> registrar_;
  };
}

#endif // phlex_core_registration_api_hpp
