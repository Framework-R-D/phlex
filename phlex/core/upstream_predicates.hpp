#ifndef PHLEX_CORE_UPSTREAM_PREDICATES_HPP
#define PHLEX_CORE_UPSTREAM_PREDICATES_HPP

#include "phlex/core/detail/maybe_predicates.hpp"
#include "phlex/core/registrar.hpp"

#include <array>
#include <concepts>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  template <typename Ptr, std::size_t NumberOutputProducts>
  class upstream_predicates {
  public:
    explicit upstream_predicates(registrar<Ptr> reg, configuration const* config) :
      registrar_{std::move(reg)}
    {
      if (!config) {
        return;
      }
      registrar_.set_predicates(detail::maybe_predicates(config));
    }

    auto& when(std::vector<std::string> predicates)
    {
      if (!registrar_.has_predicates()) {
        registrar_.set_predicates(std::move(predicates));
      }
      return *this;
    }

    auto& when(std::convertible_to<std::string> auto&&... names)
    {
      return when({std::forward<decltype(names)>(names)...});
    }

    template <std::size_t M>
      requires(NumberOutputProducts > 0)
    void output_products(std::array<std::string, M> outputs)
    {
      static_assert(
        NumberOutputProducts == M,
        "The number of specified products is not the same as the number of returned output "
        "objects.");
      registrar_.set_output_products(std::vector(outputs.begin(), outputs.end()));
    }

    void output_products(std::convertible_to<std::string> auto&&... ts)
    {
      constexpr std::size_t num_products = sizeof...(ts);
      output_products(std::array<std::string, num_products>{std::forward<decltype(ts)>(ts)...});
    }

  private:
    registrar<Ptr> registrar_;
  };
}

#endif // PHLEX_CORE_UPSTREAM_PREDICATES_HPP
