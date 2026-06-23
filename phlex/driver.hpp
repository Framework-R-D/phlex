#ifndef PHLEX_DRIVER_HPP
#define PHLEX_DRIVER_HPP

#include "phlex/configuration.hpp"
#include "phlex/core/source.hpp"
#include "phlex/detail/plugin_macros.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/fixed_hierarchy.hpp"
#include "phlex/utilities/resumable_driver.hpp"

#include "boost/mp11/algorithm.hpp"

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace phlex::experimental {
  class driver_proxy;
  struct driver_bundle;

  namespace detail {
    using next_index_t = std::function<void(framework_driver&)>;
    // Shim type for the extern "C" entry-point: out-parameter avoids returning a C++ type
    // across a C-linkage boundary.
    using driver_shim_t = void(driver_proxy, configuration const&, driver_bundle*);

    template <typename SourceParameters, typename F, typename FirstArg>
    void invoke_driver_with_sources(F& f,
                                    FirstArg&& first_arg,
                                    std::vector<source const*> const& sources)
    {
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        f(std::forward<FirstArg>(first_arg),
          dynamic_cast<std::remove_cvref_t<mp11::mp_at_c<SourceParameters, Is>> const&>(
            *sources[Is])...);
      }(std::make_index_sequence<mp11::mp_size<SourceParameters>::value>{});
    }
  };

  /// @brief Bundles the driver function and data hierarchy for the framework.
  struct driver_bundle {
    detail::next_index_t driver; ///< Driver function that advances data cells.
    fixed_hierarchy hierarchy;   ///< Data hierarchy traversed by the driver.
  };

  template <typename T>
  using is_derived_from_source = std::is_base_of<source, std::remove_cvref_t<T>>;

  template <typename F, typename FirstArg>
  concept is_driver_like_with_sources =
    check_parameters<F, FirstArg>::value &&
    mp11::mp_all_of<skip_first_type<function_parameter_types<F>>, is_derived_from_source>::value;

  template <typename Tuple>
  using source_parameter_types = skip_first_type<Tuple>;
}

namespace phlex::experimental {
  template <typename F>
  concept is_driver_like_with_cursor = is_driver_like_with_sources<F, data_cell_cursor>;

  template <typename F>
  concept is_driver_like_with_yielder = is_driver_like_with_sources<F, data_cell_yielder>;

  template <typename T>
  concept is_driver_generator_like = requires(T& generator, data_cell_yielder const& yielder) {
    { generator.hierarchy() } -> std::same_as<fixed_hierarchy>;
    { generator.driver_function() } -> std::invocable<data_cell_yielder const&>;
  };

  /// @brief Proxy for constructing a driver bundle from a user-supplied driver function.
  ///
  /// Passed to @c PHLEX_REGISTER_DRIVER plugin entry points. Users never
  /// construct this type directly.
  class driver_proxy {
  public:
    explicit driver_proxy(std::vector<source const*> sources) : sources_(std::move(sources)) {}

    /// @brief Creates a driver_bundle from a hierarchy and a user-supplied driver function.
    ///
    /// @param hierarchy  The data hierarchy the driver will traverse.
    /// @param driver_function  A callable receiving a @c data_cell_cursor const& that
    ///                         emits data cells to the framework.
    driver_bundle driver(fixed_hierarchy hierarchy,
                         is_driver_like_with_cursor auto driver_function) const
    {
      return make_driver_bundle(
        std::move(hierarchy),
        std::move(driver_function),
        [](fixed_hierarchy const& h, framework_driver& d) { return h.yield_job(d); });
    }

    /// @brief Creates a driver_bundle from a hierarchy and a user-supplied driver function.
    ///
    /// @param hierarchy  The data hierarchy the driver will traverse.
    /// @param driver_function  A callable receiving a @c data_cell_yielder const& that
    ///                         emits data cells to the framework.
    driver_bundle driver(fixed_hierarchy hierarchy,
                         is_driver_like_with_yielder auto driver_function) const
    {
      return make_driver_bundle(
        std::move(hierarchy),
        std::move(driver_function),
        [](fixed_hierarchy const& h, framework_driver& d) { return h.yielder(d); });
    }

    template <typename Generator>
      requires is_driver_generator_like<Generator>
    driver_bundle driver(std::shared_ptr<Generator> generator) const
    {
      if (!generator) {
        throw std::invalid_argument("Cannot configure driver with an empty generator.");
      }

      auto hierarchy = generator->hierarchy();
      auto generator_driver = generator->driver_function();

      return {[generator = std::move(generator),
               h = hierarchy,
               generator_driver = std::move(generator_driver)](framework_driver& d) mutable {
                std::invoke(generator_driver, h.yielder(d));
              },
              std::move(hierarchy)};
    }

  private:
    template <typename SourceParameters>
    void verify_source_parameter_count() const
    {
      if (mp11::mp_size<SourceParameters>::value != sources_.size()) {
        throw std::invalid_argument("Number of source parameters of driver function does not match "
                                    "the number of sources specified in the configuration.");
      }
    }

    template <typename DriverFunction, typename FirstArgFactory>
    driver_bundle make_driver_bundle(fixed_hierarchy hierarchy,
                                     DriverFunction driver_function,
                                     FirstArgFactory first_arg_factory) const
    {
      using driver_function_t = std::remove_cvref_t<DriverFunction>;
      using source_parameters_t =
        source_parameter_types<function_parameter_types<driver_function_t>>;

      verify_source_parameter_count<source_parameters_t>();

      auto h = hierarchy;
      return {[f = std::move(driver_function),
               h = std::move(h),
               srcs = std::move(sources_),
               first_arg_factory = std::move(first_arg_factory)](framework_driver& d) mutable {
                detail::invoke_driver_with_sources<source_parameters_t>(
                  f, first_arg_factory(h, d), srcs);
              },
              std::move(hierarchy)};
    }

    std::vector<source const*> sources_;
  };
}

#define PHLEX_REGISTER_DRIVER(...)                                                                 \
  PHLEX_DETAIL_REGISTER_DRIVER_PLUGIN(create, create_driver, __VA_ARGS__)

#endif // PHLEX_DRIVER_HPP
