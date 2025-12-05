#ifndef TEST_MOCK_WORKFLOW_ALGORITHM_HPP
#define TEST_MOCK_WORKFLOW_ALGORITHM_HPP

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "test/mock-workflow/timed_busy.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <string>
#include <tuple>

namespace phlex::experimental::test {
  template <typename T>
  constexpr std::size_t output_size = 1ull;

  template <typename... Ts>
  constexpr std::size_t output_size<std::tuple<Ts...>> = sizeof...(Ts);

  template <typename T>
  struct ensure_tuple_impl {
    using type = std::tuple<T>;
  };

  template <typename... Ts>
  struct ensure_tuple_impl<std::tuple<Ts...>> {
    using type = std::tuple<Ts...>;
  };

  template <typename... Ts>
  using ensure_tuple = typename ensure_tuple_impl<Ts...>::type;

  template <typename Input, std::default_initializable Outputs>
  class algorithm {};

  template <typename... Inputs, std::default_initializable Outputs>
  class algorithm<std::tuple<Inputs...>, Outputs> {
  public:
    explicit algorithm(std::string const& label, unsigned const duration) :
      label_{label}, duration_{duration}
    {
    }

    Outputs execute(Inputs const&...) const
    {
      timed_busy(duration_);
      spdlog::info("Executed for {:>10.6f} seconds ({})",
                   static_cast<double>(duration_.count()) / 1e6,
                   label_);
      return Outputs{};
    }

    using inputs = std::array<std::string, sizeof...(Inputs)>;
    using outputs = std::array<std::string, output_size<Outputs>>;

  private:
    std::string label_;
    std::chrono::microseconds duration_;
  };

  template <typename Inputs, typename Outputs, typename M>
  void define_algorithm(M& m, configuration const& c)
  {
    using inputs_t = ensure_tuple<Inputs>;
    using algorithm_t = algorithm<inputs_t, Outputs>;
    concurrency const j{c.get<unsigned>("concurrency", concurrency::unlimited.value)};

    // Convert product strings to product queries.
    // FIXME: There should be a c.get<product_query>(...) specialization
    auto input_product_strings = c.get<typename algorithm_t::inputs>("inputs");
    std::array<product_query, std::tuple_size_v<inputs_t>> queries{};
    std::ranges::transform(
      input_product_strings, queries.begin(), [](std::string const& product_string) {
        return product_query{product_string, "event"};
      });

    m.template make<algorithm_t>(c.get<std::string>("module_label"),
                                 c.get<unsigned>("duration_usec"))
      .transform("execute", &algorithm_t::execute, j)
      .input_family(std::move(queries))
      .output_products(c.get<typename algorithm_t::outputs>("outputs"));
  }
}

#endif // TEST_MOCK_WORKFLOW_ALGORITHM_HPP
