#ifndef PHLEX_DETAIL_PLUGIN_MACROS_HPP
#define PHLEX_DETAIL_PLUGIN_MACROS_HPP

#include "boost/preprocessor.hpp"

// NOLINTBEGIN(bugprone-macro-parentheses)
// `bugprone-macro-parentheses` is appropriate for expression-like macros, but these macros expand
// to C++ signatures, where parenthesizing parameters breaks parsing. We suppress the check for this
// block because line continuations make per-line suppression impractical.

// ================================================================================================
// Algorithm registration macros
#define PHLEX_DETAIL_NARGS(...) BOOST_PP_DEC(BOOST_PP_VARIADIC_SIZE(__VA_OPT__(, ) __VA_ARGS__))

#define PHLEX_DETAIL_CREATE_1ARG(token_type, func_name, m)                                         \
  void func_name(token_type<phlex::detail::void_tag> m, phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_2ARGS(token_type, func_name, m, cfg)                                   \
  void func_name(token_type<phlex::detail::void_tag> m, phlex::configuration const& cfg)

#define PHLEX_DETAIL_SELECT_SIGNATURE(token_type, func_name, ...)                                  \
  BOOST_PP_IF(BOOST_PP_EQUAL(PHLEX_DETAIL_NARGS(__VA_ARGS__), 1),                                  \
              PHLEX_DETAIL_CREATE_1ARG,                                                            \
              PHLEX_DETAIL_CREATE_2ARGS)                                                           \
  (token_type, func_name, __VA_ARGS__)

// Plugin entry-point functions are exported directly with C linkage so boost::dll::import_symbol
// can find them by name without an intermediate alias variable. The func_name parameter is
// retained for API compatibility but is unused in the expansion.
#define PHLEX_DETAIL_REGISTER_PLUGIN(token_type, func_name, dll_alias, ...)                        \
  extern "C" PHLEX_DETAIL_SELECT_SIGNATURE(token_type, dll_alias, __VA_ARGS__)

// ================================================================================================
// Registration macros for source plugins and explicit-provider plugins
//
// Source plugin entry-points cannot use extern "C" directly because the user-facing proxy types
// (providers_graph_proxy, source_graph_proxy) are C++ templates.  Instead we:
//   1. Forward-declare the user's C++ implementation (takes the proxy by reference).
//   2. Define a thin extern "C" shim that accepts source_bundle by value (matching
//      source_creator_t exactly), constructs the appropriate proxy from the bundle,
//      and calls the user's implementation.
//   3. Open the user's implementation definition for the body that follows the macro.
#define PHLEX_DETAIL_CREATE_SOURCE_1ARG(token_type, func_name, m)                                  \
  void func_name(token_type<phlex::detail::void_tag> m, phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_SOURCE_2ARGS(token_type, func_name, m, cfg)                            \
  void func_name(token_type<phlex::detail::void_tag> m, phlex::configuration const& cfg)

#define PHLEX_DETAIL_SELECT_SOURCE_SIGNATURE(token_type, func_name, ...)                           \
  BOOST_PP_IF(BOOST_PP_EQUAL(PHLEX_DETAIL_NARGS(__VA_ARGS__), 1),                                  \
              PHLEX_DETAIL_CREATE_SOURCE_1ARG,                                                     \
              PHLEX_DETAIL_CREATE_SOURCE_2ARGS)                                                    \
  (token_type, func_name, __VA_ARGS__)

#define PHLEX_DETAIL_REGISTER_SOURCE_PLUGIN(token_type, func_name, dll_alias, ...)                 \
  static PHLEX_DETAIL_SELECT_SOURCE_SIGNATURE(token_type, func_name, __VA_ARGS__);                 \
  extern "C" void dll_alias(phlex::experimental::source_bundle __phlex_bundle,                     \
                            phlex::configuration const& __phlex_config)                            \
  {                                                                                                \
    func_name(token_type<phlex::detail::void_tag>{__phlex_bundle}, __phlex_config);                \
  }                                                                                                \
  PHLEX_DETAIL_SELECT_SOURCE_SIGNATURE(token_type, func_name, __VA_ARGS__)

// ================================================================================================
// Driver registration plugin macros
#define PHLEX_DETAIL_CREATE_DRIVER_1ARG(func_name, d)                                              \
  phlex::experimental::driver_bundle func_name(phlex::experimental::driver_proxy d,                \
                                               phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_DRIVER_2ARGS(func_name, d, cfg)                                        \
  phlex::experimental::driver_bundle func_name(phlex::experimental::driver_proxy d,                \
                                               phlex::configuration const& cfg)

#define PHLEX_DETAIL_SELECT_DRIVER_SIGNATURE(func_name, ...)                                       \
  BOOST_PP_IF(BOOST_PP_EQUAL(PHLEX_DETAIL_NARGS(__VA_ARGS__), 1),                                  \
              PHLEX_DETAIL_CREATE_DRIVER_1ARG,                                                     \
              PHLEX_DETAIL_CREATE_DRIVER_2ARGS)                                                    \
  (func_name, __VA_ARGS__)

// The driver entry-point cannot use extern "C" directly because driver_bundle is a C++ type.
// Instead we forward-declare the user's C++ implementation, define a thin extern "C" shim that
// writes the result through an out-parameter (which has a void return type, compatible with C
// linkage), and then open the user's implementation definition for the body that follows.
#define PHLEX_DETAIL_REGISTER_DRIVER_PLUGIN(func_name, dll_alias, ...)                             \
  static PHLEX_DETAIL_SELECT_DRIVER_SIGNATURE(func_name, __VA_ARGS__);                             \
  extern "C" void dll_alias(phlex::experimental::driver_proxy __phlex_proxy,                       \
                            phlex::configuration const& __phlex_config,                            \
                            phlex::experimental::driver_bundle* __phlex_out)                       \
  {                                                                                                \
    *__phlex_out = func_name(__phlex_proxy, __phlex_config);                                       \
  }                                                                                                \
  PHLEX_DETAIL_SELECT_DRIVER_SIGNATURE(func_name, __VA_ARGS__)
// NOLINTEND(bugprone-macro-parentheses)

#endif // PHLEX_DETAIL_PLUGIN_MACROS_HPP
