#ifndef PHLEX_CORE_REGISTRATION_API_HPP
#define PHLEX_CORE_REGISTRATION_API_HPP

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
    using node_ptr = typename hof_type::node_ptr_type;

    static constexpr auto number_inputs = AlgorithmBits::number_inputs;
    static constexpr auto number_output_products = hof_type::number_output_products;

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
      registrar_{nodes.registrar_for<node_ptr>(errors)}
    {
    }

    auto input_family(std::array<specified_label, number_inputs> input_args)
    {
      if constexpr (number_output_products == 0ull) {
        // NOLINTBEGIN(performance-unnecessary-value-param)
        registrar_.set_creator(
          [this, inputs = std::move(input_args)](std::vector<std::string> predicates,
                                                 std::vector<std::string> /* output_products */
          ) {
            return std::make_unique<hof_type>(std::move(name_),
                                              concurrency_.value,
                                              std::move(predicates),
                                              graph_,
                                              std::move(alg_),
                                              std::vector(inputs.begin(), inputs.end()));
          });
        // NOLINTEND(performance-unnecessary-value-param)
      } else {
        registrar_.set_creator(
          [this, inputs = std::move(input_args)](std::vector<std::string> predicates,
                                                 std::vector<std::string> output_products) {
            return std::make_unique<hof_type>(std::move(name_),
                                              concurrency_.value,
                                              std::move(predicates),
                                              graph_,
                                              std::move(alg_),
                                              std::vector(inputs.begin(), inputs.end()),
                                              std::move(output_products));
          });
      }
      return upstream_predicates<node_ptr, number_output_products>{std::move(registrar_), config_};
    }

    template <label_compatible L>
    auto input_family(std::array<L, number_inputs> const& input_args)
    {
      return input_family(to_labels(input_args));
    }

    auto input_family(label_compatible auto... input_args)
    {
      static_assert(number_inputs == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return input_family({specified_label::create(std::move(input_args))...});
    }

  private:
    configuration const* config_;
    algorithm_name name_;
    AlgorithmBits alg_;
    concurrency concurrency_;
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    registrar<node_ptr> registrar_;
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
    using init_tuple = std::tuple<InitArgs...>;

    static constexpr auto number_inputs = AlgorithmBits::number_inputs;
    static constexpr auto number_output_products = 1; // For now

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

    auto input_family(std::array<specified_label, number_inputs - 1> input_args)
    {
      registrar_.set_creator(
        [this, inputs = std::move(input_args)](std::vector<std::string> predicates,
                                               std::vector<std::string> output_products) {
          return std::make_unique<fold_node<AlgorithmBits, init_tuple>>(
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
      return upstream_predicates<declared_fold_ptr, number_output_products>{std::move(registrar_),
                                                                            config_};
    }

    template <label_compatible L>
    auto input_family(std::array<L, number_inputs> input_args)
    {
      return input_family(to_labels(input_args));
    }

    auto input_family(label_compatible auto... input_args)
    {
      static_assert(number_inputs - 1 == sizeof...(input_args),
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
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string partition_;
    init_tuple init_;
    registrar<declared_fold_ptr> registrar_;
  };

  // ====================================================================================
  // Unfold API

  template <typename Object, typename Predicate, typename Unfold>
  class unfold_api {
    using input_parameter_types = constructor_parameter_types<Object>;

    static constexpr auto number_inputs = std::tuple_size_v<input_parameter_types>;
    static constexpr std::size_t number_output_products = number_output_objects<Unfold>;

    // FIXME: Should maybe use some type of static assert, but not in a way that
    //        constrains the arguments of the Predicate and the Unfold to be the same.
    //
    // static_assert(
    //   std::same_as<function_parameter_types<Predicate>, function_parameter_types<Unfold>>);

  public:
    unfold_api(configuration const* config,
               std::string name,
               Predicate predicate,
               Unfold unfold,
               concurrency c,
               tbb::flow::graph& g,
               node_catalog& nodes,
               std::vector<std::string>& errors,
               std::string destination_data_layer) :
      config_{config},
      registrar_{nodes.registrar_for<declared_unfold_ptr>(errors)},
      name_{detail::make_algorithm_name(config, std::move(name))},
      concurrency_{c.value},
      graph_{g},
      predicate_{std::move(predicate)},
      unfold_{std::move(unfold)},
      destination_layer_{std::move(destination_data_layer)}
    {
    }

    auto input_family(std::array<specified_label, number_inputs> input_args)
    {
      registrar_.set_creator(
        [this, inputs = std::move(input_args)](std::vector<std::string> upstream_predicates,
                                               std::vector<std::string> output_products) {
          return std::make_unique<unfold_node<Object, Predicate, Unfold>>(
            std::move(name_),
            concurrency_,
            std::move(upstream_predicates),
            graph_,
            std::move(predicate_),
            std::move(unfold_),
            std::vector(inputs.begin(), inputs.end()),
            std::move(output_products),
            std::move(destination_layer_));
        });
      return upstream_predicates<declared_unfold_ptr, number_output_products>{std::move(registrar_),
                                                                              config_};
    }

    auto input_family(label_compatible auto... input_args)
    {
      static_assert(number_inputs == sizeof...(input_args),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");
      return input_family({specified_label{std::forward<decltype(input_args)>(input_args)}...});
    }

  private:
    configuration const* config_;
    registrar<declared_unfold_ptr> registrar_;
    algorithm_name name_;
    std::size_t concurrency_;
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    Predicate predicate_;
    Unfold unfold_;
    std::string destination_layer_;
  };

  // ====================================================================================
  // Output API

  class output_api {
  public:
    output_api(registrar<declared_output_ptr> reg,
               configuration const* config,
               std::string name,
               tbb::flow::graph& g,
               detail::output_function_t&& f,
               concurrency c);

    void when(std::vector<std::string> predicates);

    void when(std::convertible_to<std::string> auto&&... names)
    {
      when({std::forward<decltype(names)>(names)...});
    }

  private:
    algorithm_name name_;
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    detail::output_function_t ft_;
    concurrency concurrency_;
    registrar<declared_output_ptr> reg_;
  };
}

#endif // PHLEX_CORE_REGISTRATION_API_HPP
