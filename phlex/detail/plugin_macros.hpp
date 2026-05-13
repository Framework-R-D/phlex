#ifndef PHLEX_DETAIL_PLUGIN_MACROS_HPP
#define PHLEX_DETAIL_PLUGIN_MACROS_HPP

#include "boost/preprocessor.hpp"

// NOLINTBEGIN(bugprone-macro-parentheses)
// `bugprone-macro-parentheses` is appropriate for expression-like macros, but these macros expand
// to C++ signatures, where parenthesizing parameters breaks parsing. We suppress the check for this
// block because line continuations make per-line suppression impractical.
#define PHLEX_DETAIL_NARGS(...) BOOST_PP_DEC(BOOST_PP_VARIADIC_SIZE(__VA_OPT__(, ) __VA_ARGS__))

#define PHLEX_DETAIL_CREATE_1ARG(token_type, func_name, m)                                         \
  void func_name(token_type<phlex::experimental::void_tag>& m, phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_2ARGS(token_type, func_name, m, pset)                                  \
  void func_name(token_type<phlex::experimental::void_tag>& m, phlex::configuration const& config)

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

#define PHLEX_DETAIL_CREATE_DRIVER_1ARG(func_name, d)                                              \
  phlex::experimental::driver_bundle func_name(phlex::experimental::driver_proxy const& d,         \
                                               phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_DRIVER_2ARGS(func_name, d, config)                                     \
  phlex::experimental::driver_bundle func_name(phlex::experimental::driver_proxy const& d,         \
                                               phlex::configuration const& config)

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
  extern "C" void dll_alias(phlex::experimental::driver_proxy const& __phlex_proxy,                \
                            phlex::configuration const& __phlex_config,                            \
                            phlex::experimental::driver_bundle* __phlex_out)                       \
  {                                                                                                \
    *__phlex_out = func_name(__phlex_proxy, __phlex_config);                                       \
  }                                                                                                \
  PHLEX_DETAIL_SELECT_DRIVER_SIGNATURE(func_name, __VA_ARGS__)
// NOLINTEND(bugprone-macro-parentheses)

#endif // PHLEX_DETAIL_PLUGIN_MACROS_HPP
