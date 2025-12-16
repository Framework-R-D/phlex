#ifndef PHLEX_SOURCE_HPP
#define PHLEX_SOURCE_HPP

#include "boost/dll/alias.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/graph_proxy.hpp"

#include "boost/preprocessor.hpp"

namespace phlex::experimental {
  template <typename T>
  class source_token : graph_proxy<T> {
    using base = graph_proxy<T>;

  public:
    using base::graph_proxy;

    using base::make;

    using base::provide;
  };

  namespace detail {
    using source_creator_t = void(graph_proxy<void_tag>&, configuration const&);
  }
}

#define SOURCE_NARGS(...) BOOST_PP_DEC(BOOST_PP_VARIADIC_SIZE(__VA_OPT__(, ) __VA_ARGS__))

#define CREATE_SOURCE_1ARG(m)                                                                      \
  void create_src(phlex::experimental::source_token<phlex::experimental::void_tag>& m,             \
                  phlex::experimental::configuration const&)
#define CREATE_SOURCE_2ARGS(m, pset)                                                               \
  void create_src(phlex::experimental::source_token<phlex::experimental::void_tag>& m,             \
                  phlex::experimental::configuration const& config)

#define SELECT_SOURCE_SIGNATURE(...)                                                               \
  BOOST_PP_IF(BOOST_PP_EQUAL(SOURCE_NARGS(__VA_ARGS__), 1),                                        \
              CREATE_SOURCE_1ARG,                                                                  \
              CREATE_SOURCE_2ARGS)(__VA_ARGS__)

#define PHLEX_EXPERIMENTAL_REGISTER_PROVIDERS(...)                                                 \
  static SELECT_SOURCE_SIGNATURE(__VA_ARGS__);                                                     \
  BOOST_DLL_ALIAS(create_src, create_source)                                                       \
  SELECT_SOURCE_SIGNATURE(__VA_ARGS__)

#endif // PHLEX_SOURCE_HPP
