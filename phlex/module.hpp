#ifndef PHLEX_MODULE_HPP
#define PHLEX_MODULE_HPP

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/registration_context.hpp"
#include "phlex/detail/plugin_macros.hpp"

#include <utility>

namespace phlex::detail {
  /// @brief Proxy for registering module algorithm nodes.
  ///
  /// Passed to @c PHLEX_REGISTER_ALGORITHMS plugin entry points. Provides
  /// access to fold, observe, output, predicate, transform, and unfold registration.
  /// Users never construct this type directly.
  template <typename T>
  class module_graph_proxy {
  public:
    template <typename U, typename... Args>
    module_graph_proxy<U> make(Args&&... args) const
      requires(not is_bound_object<T>)
    {
      return module_graph_proxy<U>{state_.template bind<U>(std::forward<Args>(args)...)};
    }

    template <typename... InitArgs>
    auto fold(std::string_view name,
              is_fold_like auto f,
              concurrency c = concurrency::serial,
              std::string partition = "job",
              InitArgs&&... init_args) const
    {
      return state_.make_glue().fold(
        name, std::move(f), c, std::move(partition), std::forward<InitArgs>(init_args)...);
    }

    auto observe(std::string_view name,
                 is_observer_like auto f,
                 concurrency c = concurrency::serial) const
    {
      return state_.make_glue().observe(name, std::move(f), c);
    }

    auto output(std::string_view name,
                is_output_like auto f,
                concurrency c = concurrency::serial) const
    {
      return state_.make_glue().output(name, std::move(f), c);
    }

    auto predicate(std::string_view name,
                   is_predicate_like auto f,
                   concurrency c = concurrency::serial) const
    {
      return state_.make_glue().predicate(name, std::move(f), c);
    }

    auto transform(std::string_view name,
                   is_transform_like auto f,
                   concurrency c = concurrency::serial) const
    {
      return state_.make_glue().transform(name, std::move(f), c);
    }

    template <typename Unfolder>
    auto unfold(std::string_view name,
                is_predicate_like auto pred,
                auto unf,
                std::string destination_data_layer,
                concurrency c = concurrency::serial) const
    {
      return state_.template make_unbound_glue<Unfolder>().unfold(
        name, std::move(pred), std::move(unf), c, std::move(destination_data_layer));
    }

  private:
    friend class internal::proxy_factory;
    template <typename>
    friend class module_graph_proxy;

    explicit module_graph_proxy(internal::registration_state<T> state) : state_{std::move(state)} {}

    internal::registration_state<T> state_;
  };

  inline module_graph_proxy<void_tag> internal::proxy_factory::module_proxy(
    registration_context context)
  {
    return module_graph_proxy<void_tag>{registration_state<void_tag>{context}};
  }

  namespace internal {
    using module_creator_t = void(module_graph_proxy<void_tag> const&, configuration const&);
  }
}

#define PHLEX_REGISTER_ALGORITHMS(...)                                                             \
  PHLEX_DETAIL_REGISTER_PLUGIN(                                                                    \
    phlex::detail::module_graph_proxy, create, create_module, __VA_ARGS__)

#endif // PHLEX_MODULE_HPP
