#ifndef PHLEX_DRIVER_HPP
#define PHLEX_DRIVER_HPP

#include "phlex/configuration.hpp"
#include "phlex/core/source.hpp"
#include "phlex/detail/plugin_macros.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/fixed_hierarchy.hpp"
#include "phlex/utilities/resumable_driver.hpp"

#include "boost/core/demangle.hpp"
#include "boost/mp11/algorithm.hpp"
#include "fmt/format.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace phlex::experimental {
  class driver_proxy;
  struct driver_bundle;

  namespace detail {
    using next_index_t = std::function<void(phlex::detail::framework_driver&)>;
    // Shim type for the extern "C" entry-point: out-parameter avoids returning a C++ type
    // across a C-linkage boundary.
    using driver_shim_t = void(driver_proxy, configuration const&, driver_bundle*);

    template <typename SourceType>
    std::remove_cvref_t<SourceType> const& as_driver_source(source const* src, std::size_t index)
    {
      assert(src != nullptr);

      using expected_source_t = std::remove_cvref_t<SourceType>;

      if (auto const* casted = dynamic_cast<expected_source_t const*>(src)) {
        return *casted;
      }

      throw std::runtime_error(
        fmt::format("Driver source type mismatch at source index {}: expected '{}' but got '{}'.",
                    index,
                    boost::core::demangle(typeid(expected_source_t).name()),
                    boost::core::demangle(typeid(*src).name())));
    }

    template <typename SourceParameters, typename F, typename FirstArg>
    void invoke_driver_with_sources(F& f,
                                    FirstArg&& first_arg,
                                    std::vector<source const*> const& sources)
    {
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        f(std::forward<FirstArg>(first_arg),
          as_driver_source<mp11::mp_at_c<SourceParameters, Is>>(sources[Is], Is)...);
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

  template <typename F>
  concept is_driver_like = is_driver_like_with_sources<F, data_cell_cursor> ||
                           is_driver_like_with_sources<F, data_cell_yielder>;

  template <typename T>
  concept is_driver_builder_like = requires(T& driver_builder) {
    { driver_builder.hierarchy() } -> std::same_as<fixed_hierarchy>;
    { driver_builder.driver_function() } -> is_driver_like;
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
      return make_driver_bundle(std::move(hierarchy),
                                std::move(driver_function),
                                [](fixed_hierarchy const& h, phlex::detail::framework_driver& d) {
                                  return h.yield_job(d);
                                });
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
        [](fixed_hierarchy const& h, phlex::detail::framework_driver& d) { return h.yielder(d); });
    }

    template <typename DriverBuilder>
      requires is_driver_builder_like<DriverBuilder>
    driver_bundle driver(std::shared_ptr<DriverBuilder> driver_builder) const
    {
      if (!driver_builder) {
        throw std::invalid_argument("Cannot configure driver with an empty driver builder.");
      }

      auto hierarchy = driver_builder->hierarchy();
      auto driver_function = driver_builder->driver_function();

      using first_argument_type = std::remove_cvref_t<decltype(driver_function)>;
      auto first_arg_factory = [hierarchy = hierarchy](fixed_hierarchy const& h,
                                                       phlex::detail::framework_driver& d) {
        if constexpr (std::is_same_v<first_argument_type, data_cell_cursor>) {
          return h.yield_job(d);
        } else {
          return h.yielder(d);
        }
      };

      return make_driver_bundle(
        std::move(hierarchy), std::move(driver_function), std::move(first_arg_factory));
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
               first_arg_factory =
                 std::move(first_arg_factory)](phlex::detail::framework_driver& d) mutable {
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
