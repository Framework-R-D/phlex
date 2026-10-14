#ifndef PHLEX_CORE_GLUE_HPP
#define PHLEX_CORE_GLUE_HPP

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/registration_api.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/core/source.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/phlex_core_export.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace phlex {
  class configuration;
}

namespace phlex::detail {
  struct node_catalog;
  class source;
  template <typename>
  class glue;

  namespace internal {
    PHLEX_CORE_EXPORT void verify_name(std::string_view name, configuration const* config);
    template <typename>
    class registration_state;
  }

  // ==============================================================================
  // Registering user functions
  /**
 * @brief A class template that provides a fluent interface for registering data processing nodes in a flow graph.
 *
 * The glue class acts as a registration helper that allows binding user-defined functions and algorithms
 * to nodes in a TBB flow graph. It provides methods to create different types of processing nodes like
 * fold, observe, output, predicate, transform, and unfold.
 *
 * @tparam T The type of the object that contains the user-defined functions/algorithms to be registered.
 *           This object is stored as a shared pointer and its methods are bound to the created nodes.
 */
  namespace internal {
    class PHLEX_CORE_EXPORT registration_core {
    private:
      template <typename>
      friend class phlex::detail::glue;
      template <typename>
      friend class internal::registration_state;

      registration_core(configuration const* config,
                        tbb::flow::graph& graph,
                        phlex::experimental::identifier const& stage,
                        node_catalog& nodes,
                        std::vector<std::string>& errors,
                        resource_catalog& resources);

      void verify_name(std::string_view name) const;

      template <typename AlgorithmBits, typename... InitArgs>
      auto fold(std::string_view name,
                AlgorithmBits alg,
                concurrency c,
                std::string partition,
                InitArgs&&... init_args)
      {
        return fold_api{config_,
                        name,
                        stage_,
                        std::move(alg),
                        c,
                        graph_,
                        nodes_,
                        errors_,
                        resources_,
                        std::move(partition),
                        std::forward<InitArgs>(init_args)...};
      }

      template <typename AlgorithmBits>
      auto observe(std::string_view name, AlgorithmBits alg, concurrency c)
      {
        return make_registration<observer_node, declared_observer_ptr>(
          config_, name, stage_, std::move(alg), c, graph_, nodes_, errors_, resources_);
      }

      template <typename AlgorithmBits>
      auto provide(std::string_view name, AlgorithmBits alg, concurrency c)
      {
        return provider_api{
          config_, name, stage_, std::move(alg), c, graph_, nodes_, errors_, resources_};
      }

      template <typename AlgorithmBits>
      auto transform(std::string_view name, AlgorithmBits alg, concurrency c)
      {
        return make_registration<transform_node, declared_transform_ptr>(
          config_, name, stage_, std::move(alg), c, graph_, nodes_, errors_, resources_);
      }

      template <typename AlgorithmBits>
      auto predicate(std::string_view name, AlgorithmBits alg, concurrency c)
      {
        return make_registration<predicate_node, declared_predicate_ptr>(
          config_, name, stage_, std::move(alg), c, graph_, nodes_, errors_, resources_);
      }

      template <typename Object, typename Predicate, typename Unfold>
      auto unfold(std::string_view name,
                  Predicate predicate,
                  Unfold unfold,
                  concurrency c,
                  std::string destination_data_layer)
      {
        return unfold_api<Object, Predicate, Unfold>{config_,
                                                     name,
                                                     stage_,
                                                     std::move(predicate),
                                                     std::move(unfold),
                                                     c,
                                                     graph_,
                                                     nodes_,
                                                     errors_,
                                                     resources_,
                                                     std::move(destination_data_layer)};
      }

      output_api output(std::string_view name, internal::output_function_t f, concurrency c);

      template <std::derived_from<source> Source, typename... Args>
      void add_source(std::string_view name, Args&&... args)
      {
        insert_source(name, std::make_unique<Source>(std::forward<Args>(args)...));
      }

      void insert_source(std::string_view name, std::unique_ptr<source> source);

      configuration const* config_;
      // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
      tbb::flow::graph& graph_;
      phlex::experimental::identifier const& stage_;
      node_catalog& nodes_;
      std::vector<std::string>& errors_;
      resource_catalog& resources_;
      // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
    };
  }

  template <typename T>
  class glue {
  public:
    // 'f' is a by-value sink: it is moved into algorithm_bits.  The clang-tidy
    // warning to take it by const reference is a false positive.
    template <typename... InitArgs>
    auto fold(std::string_view name,
              auto f, // NOLINT(performance-unnecessary-value-param)
              concurrency c,
              std::string partition,
              InitArgs&&... init_args)
    {
      core_.verify_name(name);
      return core_.fold(name,
                        algorithm_bits(bound_obj_, std::move(f)),
                        c,
                        std::move(partition),
                        std::forward<InitArgs>(init_args)...);
    }

    // 'f' is a by-value sink: it is moved into algorithm_bits.  The clang-tidy
    // warning to take it by const reference is a false positive.
    template <typename FT>
    auto observe(std::string_view name,
                 FT f, // NOLINT(performance-unnecessary-value-param)
                 concurrency c)
    {
      core_.verify_name(name);
      return core_.observe(name, algorithm_bits{bound_obj_, std::move(f)}, c);
    }

    // 'f' is a by-value sink: it is moved into algorithm_bits.  The clang-tidy
    // warning to take it by const reference is a false positive.
    template <typename FT>
    auto provide(std::string_view name,
                 FT f, // NOLINT(performance-unnecessary-value-param)
                 concurrency c)
    {
      core_.verify_name(name);
      return core_.provide(name, algorithm_bits{bound_obj_, std::move(f)}, c);
    }

    // 'f' is a by-value sink: it is moved into algorithm_bits.  The clang-tidy
    // warning to take it by const reference is a false positive.
    template <typename FT>
    auto transform(std::string_view name,
                   FT f, // NOLINT(performance-unnecessary-value-param)
                   concurrency c)
    {
      core_.verify_name(name);
      return core_.transform(name, algorithm_bits{bound_obj_, std::move(f)}, c);
    }

    // 'f' is a by-value sink: it is moved into algorithm_bits.  The clang-tidy
    // warning to take it by const reference is a false positive.
    template <typename FT>
    auto predicate(std::string_view name,
                   FT f, // NOLINT(performance-unnecessary-value-param)
                   concurrency c)
    {
      core_.verify_name(name);
      return core_.predicate(name, algorithm_bits{bound_obj_, std::move(f)}, c);
    }

    auto unfold(std::string_view name,
                auto predicate,
                auto unfold,
                concurrency c,
                std::string destination_data_layer)
    {
      assert(!bound_obj_);
      core_.verify_name(name);
      return core_.template unfold<T>(
        name, std::move(predicate), std::move(unfold), c, std::move(destination_data_layer));
    }

    auto output(std::string_view name, is_output_like auto f, concurrency c = concurrency::serial)
    {
      return core_.output(name, delegate(bound_obj_, f), c);
    }

    template <std::derived_from<source> Source, typename... Args>
    void add_source(std::string_view name, Args&&... args)
    {
      core_.template add_source<Source>(name, std::forward<Args>(args)...);
    }

  private:
    template <typename>
    friend class internal::registration_state;

    glue(internal::registration_core core, std::shared_ptr<T> bound_obj) :
      core_{std::move(core)}, bound_obj_{std::move(bound_obj)}
    {
    }

    internal::registration_core core_;
    std::shared_ptr<T> bound_obj_;
  };
}

#endif // PHLEX_CORE_GLUE_HPP
