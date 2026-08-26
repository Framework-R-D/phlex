#ifndef PHLEX_CORE_REGISTRATION_API_HPP
#define PHLEX_CORE_REGISTRATION_API_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/declared_fold.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/core/upstream_predicates.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace phlex {
  class configuration;
}

namespace phlex::detail {

  template <typename T>
  concept selector_or_resource =
    std::same_as<std::remove_cvref_t<T>, product_selector> || is_resource<T>;

  template <typename Split, typename... Args>
  auto partition_selectors_and_resources(Args&&... args)
  {
    auto all = std::forward_as_tuple(std::forward<Args>(args)...);
    auto selectors = [&all]<std::size_t... Is>(std::index_sequence<Is...>) {
      return std::array<product_selector, Split::number_selectors>{std::move(std::get<Is>(all))...};
    }(std::make_index_sequence<Split::number_selectors>{});
    auto resources = [&all]<std::size_t... Is>(std::index_sequence<Is...>) {
      return
        typename Split::resources_type{std::move(std::get<Is + Split::number_selectors>(all))...};
    }(std::make_index_sequence<Split::number_resources>{});
    return std::pair{std::move(selectors), std::move(resources)};
  }

  // ====================================================================================
  // Registration API

  template <template <typename...> typename HOF, typename AlgorithmBits>
  class registration_api {
    using hof_type = HOF<AlgorithmBits>;
    using node_ptr = hof_type::node_ptr_type;
    using input_parameter_types = AlgorithmBits::input_parameter_types;

    static constexpr auto num_inputs = AlgorithmBits::number_inputs;
    static constexpr auto num_outputs = hof_type::number_output_products;

    static_assert(num_inputs > 0, "input_family requires at least one product selector.");

  public:
    registration_api(configuration const* config,
                     std::string_view name,
                     AlgorithmBits alg,
                     concurrency c,
                     tbb::flow::graph& g,
                     node_catalog& nodes,
                     std::vector<std::string>& errors,
                     resource_catalog& resources) :
      config_{config},
      name_{phlex::experimental::internal::make_algorithm_name(config, name)},
      alg_{std::move(alg)},
      concurrency_{c},
      graph_{g},
      registrar_{nodes.registrar_for<node_ptr>(errors)},
      resources_{resources}
    {
    }

    template <std::size_t NProducts, typename... Resources>
    auto input_family(std::array<product_selector, NProducts> input_args, std::tuple<Resources...>)
    {
      static_assert((is_resource<Resources> && ...),
                    "The second argument to input_family(...) must be a tuple of resources.");
      static_assert(num_inputs == NProducts + sizeof...(Resources),
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");

      populate_types<boost::mp11::mp_take_c<input_parameter_types, NProducts>>(input_args);

      if constexpr (num_outputs == 0ull) {
        registrar_.set_creator([this, inputs = std::move(input_args)](
                                 auto predicates, auto const& /* output_product_suffixes */) {
          using node_type = HOF<AlgorithmBits, internal::resource_type_t<Resources>...>;
          return std::make_unique<node_type>(std::move(name_),
                                             concurrency_.value,
                                             std::move(predicates),
                                             graph_,
                                             std::move(alg_),
                                             std::vector(std::from_range, std::move(inputs)),
                                             resources_);
        });
      } else {
        registrar_.set_creator(
          [this, inputs = std::move(input_args)](auto predicates, auto output_product_suffixes) {
            using node_type = HOF<AlgorithmBits, internal::resource_type_t<Resources>...>;
            return std::make_unique<node_type>(std::move(name_),
                                               concurrency_.value,
                                               std::move(predicates),
                                               graph_,
                                               std::move(alg_),
                                               std::vector(std::from_range, std::move(inputs)),
                                               std::move(output_product_suffixes),
                                               resources_);
          });
      }
      return upstream_predicates<node_ptr, num_outputs>{std::move(registrar_), config_};
    }

    auto input_family(std::array<product_selector, num_inputs> input_args)
    {
      return input_family(std::move(input_args), std::tuple<>{});
    }

    // These values are moved into registration state; clang-tidy incorrectly reports the
    // by-value parameters as unnecessary copies.
    auto input_family(
      product_selector first_selector,              // NOLINT(performance-unnecessary-value-param)
      selector_or_resource auto... additional_args) // NOLINT(performance-unnecessary-value-param)
    {
      using split = resource_split<decltype(first_selector), decltype(additional_args)...>;
      static_assert(split::resources_are_last,
                    "All resource<T> arguments to input_family(...) must follow the "
                    "product_selector arguments.");
      static_assert(num_inputs == split::number_selectors + split::number_resources,
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");

      auto [selectors, resources] = partition_selectors_and_resources<split>(
        std::move(first_selector), std::move(additional_args)...);
      return input_family(std::move(selectors), std::move(resources));
    }

  private:
    configuration const* config_;
    phlex::experimental::algorithm_name name_;
    AlgorithmBits alg_;
    concurrency concurrency_;
    // Non-owning reference to the TBB graph; this class is a short-lived registration builder.
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    registrar<node_ptr> registrar_;
    resource_catalog& resources_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  template <template <typename...> typename HOF, typename AlgorithmBits>
  auto make_registration(configuration const* config,
                         std::string_view name,
                         AlgorithmBits alg,
                         concurrency c,
                         tbb::flow::graph& g,
                         node_catalog& nodes,
                         std::vector<std::string>& errors,
                         resource_catalog& resources)
  {
    return registration_api<HOF, AlgorithmBits>{
      config, name, std::move(alg), c, g, nodes, errors, resources};
  }

  // ====================================================================================
  // Provider API

  template <typename AlgorithmBits>
  class provider_api {
  public:
    provider_api(configuration const* config,
                 std::string_view name,
                 AlgorithmBits alg,
                 concurrency c,
                 tbb::flow::graph& g,
                 node_catalog& nodes,
                 std::vector<std::string>& errors,
                 resource_catalog& resources) :
      config_{config},
      name_{phlex::experimental::internal::make_algorithm_name(config, name)},
      alg_{std::move(alg)},
      concurrency_{c},
      graph_{g},
      registrar_{nodes.registrar_for<provider_node_ptr>(errors)},
      resources_{resources}
    {
    }

    auto output_product(phlex::experimental::algorithm_name creator,
                        phlex::experimental::identifier suffix,
                        phlex::experimental::identifier output_layer,
                        phlex::experimental::identifier stage = "CURRENT"_id)
    {
      using return_type_t = return_type<typename AlgorithmBits::algorithm_type>;
      product_specification output_spec(
        std::move(creator), std::move(suffix), make_type_id<return_type_t>());

      auto type_erased_alg = [alg = alg_.release_algorithm()](data_cell_index const& index) {
        return product_for(std::invoke(alg, index));
      };

      registrar_.set_creator([this,
                              alg = std::move(type_erased_alg),
                              output_spec = std::move(output_spec),
                              output_layer = std::move(output_layer),
                              stage = std::move(stage)](auto const& /* predicates */,
                                                        auto const& /* output_product_suffixes */) {
        return std::make_unique<provider_node>(std::move(name_),
                                               concurrency_.value,
                                               graph_,
                                               std::move(alg),
                                               std::move(output_spec),
                                               std::move(output_layer),
                                               std::move(stage));
      });
    }

  private:
    configuration const* config_;
    phlex::experimental::algorithm_name name_;
    AlgorithmBits alg_;
    concurrency concurrency_;
    // Non-owning reference to the TBB graph; this class is a short-lived registration builder.
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    registrar<provider_node_ptr> registrar_;
    resource_catalog& resources_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  // ====================================================================================
  // Fold API

  template <typename AlgorithmBits, typename... InitArgs>
  class fold_api {
    using init_tuple = std::tuple<InitArgs...>;
    using input_parameter_types = skip_first_type<typename AlgorithmBits::input_parameter_types>;

    static constexpr auto num_inputs = AlgorithmBits::number_inputs;
    static constexpr auto num_outputs = 1; // For now

    static_assert(num_inputs > 1, "input_family requires at least one product selector.");

  public:
    fold_api(configuration const* config,
             std::string_view name,
             AlgorithmBits alg,
             concurrency c,
             tbb::flow::graph& g,
             node_catalog& nodes,
             std::vector<std::string>& errors,
             resource_catalog& resources,
             std::string partition,
             InitArgs&&... init_args) :
      config_{config},
      name_{phlex::experimental::internal::make_algorithm_name(config, name)},
      alg_{std::move(alg)},
      concurrency_{c},
      graph_{g},
      partition_{std::move(partition)},
      init_{std::forward<InitArgs>(init_args)...},
      registrar_{nodes.registrar_for<declared_fold_ptr>(errors)},
      resources_{resources}
    {
    }

    template <std::size_t NProducts, typename... Resources>
    auto input_family(std::array<product_selector, NProducts> input_args, std::tuple<Resources...>)
    {
      static_assert((is_resource<Resources> && ...),
                    "The second argument to input_family(...) must be a tuple of resources.");
      static_assert(num_inputs - 1 == NProducts + sizeof...(Resources),
                    "The number of fold product parameters is not the same as the number of "
                    "specified input arguments.");

      using product_parameter_types = boost::mp11::mp_take_c<input_parameter_types, NProducts>;
      populate_types<product_parameter_types>(input_args);

      registrar_.set_creator(
        [this, inputs = std::move(input_args)](auto predicates, auto output_product_suffixes) {
          using node_type =
            fold_node<AlgorithmBits, init_tuple, internal::resource_type_t<Resources>...>;
          return std::make_unique<node_type>(std::move(name_),
                                             concurrency_.value,
                                             std::move(predicates),
                                             graph_,
                                             std::move(alg_),
                                             std::move(init_),
                                             std::vector(std::from_range, std::move(inputs)),
                                             std::move(output_product_suffixes),
                                             std::move(partition_),
                                             resources_);
        });
      return upstream_predicates<declared_fold_ptr, num_outputs>{std::move(registrar_), config_};
    }

    auto input_family(std::array<product_selector, num_inputs - 1> input_args)
    {
      return input_family(std::move(input_args), std::tuple<>{});
    }

    // These values are moved into registration state; clang-tidy incorrectly reports the
    // by-value parameters as unnecessary copies.
    auto input_family(
      product_selector first_selector,              // NOLINT(performance-unnecessary-value-param)
      selector_or_resource auto... additional_args) // NOLINT(performance-unnecessary-value-param)
    {
      using split = resource_split<decltype(first_selector), decltype(additional_args)...>;
      static_assert(split::resources_are_last,
                    "All resource<T> arguments to input_family(...) must follow the "
                    "product_selector arguments.");
      static_assert(num_inputs - 1 == split::number_selectors + split::number_resources,
                    "The number of function parameters is not the same as the number of specified "
                    "input arguments.");

      auto [selectors, resources] = partition_selectors_and_resources<split>(
        std::move(first_selector), std::move(additional_args)...);
      return input_family(std::move(selectors), std::move(resources));
    }

  private:
    configuration const* config_;
    phlex::experimental::algorithm_name name_;
    AlgorithmBits alg_;
    concurrency concurrency_;
    // Non-owning reference to the TBB graph; this class is a short-lived registration builder.
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string partition_;
    init_tuple init_;
    registrar<declared_fold_ptr> registrar_;
    resource_catalog& resources_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  // ====================================================================================
  // Unfold API

  template <typename Object, typename Predicate, typename Unfold>
  class unfold_api {
    using all_input_parameter_types = constructor_parameter_types<Object>;

    static constexpr auto num_inputs = std::tuple_size_v<all_input_parameter_types>;
    static constexpr std::size_t num_outputs = number_output_objects<Unfold>;

    static_assert(num_inputs > 0, "input_family requires at least one product selector.");

    // FIXME: Should maybe use some type of static assert, but not in a way that
    //        constrains the arguments of the Predicate and the Unfold to be the same.
    //
    // static_assert(
    //   std::same_as<function_parameter_types<Predicate>, function_parameter_types<Unfold>>);

  public:
    unfold_api(configuration const* config,
               std::string_view name,
               Predicate predicate,
               Unfold unfold,
               concurrency c,
               tbb::flow::graph& g,
               node_catalog& nodes,
               std::vector<std::string>& errors,
               resource_catalog& resources,
               std::string destination_data_layer) :
      config_{config},
      registrar_{nodes.registrar_for<declared_unfold_ptr>(errors)},
      name_{phlex::experimental::internal::make_algorithm_name(config, name)},
      concurrency_{c.value},
      graph_{g},
      predicate_{std::move(predicate)},
      unfold_{std::move(unfold)},
      destination_layer_{std::move(destination_data_layer)},
      resources_{resources}
    {
    }

    // The selectors configure Object's constructor while trailing resources configure Unfold.
    // FIXME: Evaluate whether input_family(...) should apply only to the constructor and a
    // separate registration clause should declare resources for the unfold operation.
    template <std::size_t NProducts, typename... Resources>
    auto input_family(std::array<product_selector, NProducts> input_args, std::tuple<Resources...>)
    {
      static_assert((is_resource<Resources> && ...),
                    "The second argument to input_family(...) must be a tuple of resources.");
      static_assert(num_inputs == NProducts,
                    "The number of generator constructor parameters is not the same as the number "
                    "of specified product selectors.");

      using product_parameter_types = boost::mp11::mp_take_c<all_input_parameter_types, NProducts>;
      populate_types<product_parameter_types>(input_args);

      registrar_.set_creator([this, inputs = std::move(input_args)](auto upstream_predicates,
                                                                    auto output_product_suffixes) {
        using node_type =
          unfold_node<Object, Predicate, Unfold, internal::resource_type_t<Resources>...>;
        return std::make_unique<node_type>(std::move(name_),
                                           concurrency_,
                                           std::move(upstream_predicates),
                                           graph_,
                                           std::move(predicate_),
                                           std::move(unfold_),
                                           std::vector(std::from_range, std::move(inputs)),
                                           std::move(output_product_suffixes),
                                           std::move(destination_layer_),
                                           resources_);
      });
      return upstream_predicates<declared_unfold_ptr, num_outputs>{std::move(registrar_), config_};
    }

    auto input_family(std::array<product_selector, num_inputs> input_args)
    {
      return input_family(std::move(input_args), std::tuple<>{});
    }

    // These values are moved into registration state; clang-tidy incorrectly reports the
    // by-value parameters as unnecessary copies.
    auto input_family(
      product_selector first_selector,              // NOLINT(performance-unnecessary-value-param)
      selector_or_resource auto... additional_args) // NOLINT(performance-unnecessary-value-param)
    {
      using split = resource_split<decltype(first_selector), decltype(additional_args)...>;
      static_assert(
        num_inputs == split::number_selectors,
        "The number of generator constructor parameters is not the same as the number of "
        "specified input arguments.");
      static_assert(split::resources_are_last,
                    "All resource<T> arguments to input_family(...) must follow the "
                    "product_selector arguments.");

      auto [selectors, resources] = partition_selectors_and_resources<split>(
        std::move(first_selector), std::move(additional_args)...);
      return input_family(std::move(selectors), std::move(resources));
    }

  private:
    configuration const* config_;
    registrar<declared_unfold_ptr> registrar_;
    phlex::experimental::algorithm_name name_;
    std::size_t concurrency_;
    // Non-owning reference to the TBB graph; this class is a short-lived registration builder.
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    Predicate predicate_;
    Unfold unfold_;
    std::string destination_layer_;
    resource_catalog& resources_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  // ====================================================================================
  // Output API

  class PHLEX_CORE_EXPORT output_api {
  public:
    output_api(registrar<declared_output_ptr> reg,
               configuration const* config,
               std::string_view name,
               tbb::flow::graph& g,
               internal::output_function_t&& f,
               concurrency c);

    void experimental_when(std::vector<std::string> predicates);

    void experimental_when(std::convertible_to<std::string> auto&&... names)
    {
      experimental_when({std::forward<decltype(names)>(names)...});
    }

  private:
    phlex::experimental::algorithm_name name_;
    // Non-owning reference to the TBB graph; this class is a short-lived registration builder.
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    internal::output_function_t ft_;
    concurrency concurrency_;
    registrar<declared_output_ptr> reg_;
  };
}

#endif // PHLEX_CORE_REGISTRATION_API_HPP
