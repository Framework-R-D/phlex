#ifndef PHLEX_DETAIL_PLUGIN_MACROS_HPP
#define PHLEX_DETAIL_PLUGIN_MACROS_HPP

#include <boost/preprocessor.hpp>

// NOLINTBEGIN(bugprone-macro-parentheses)
// `bugprone-macro-parentheses` is appropriate for expression-like macros, but these macros expand
// to C++ signatures, where parenthesizing parameters breaks parsing. We suppress the check for this
// block because line continuations make per-line suppression impractical.

// ================================================================================================
// Algorithm registration macros
#define PHLEX_DETAIL_NARGS(...) BOOST_PP_DEC(BOOST_PP_VARIADIC_SIZE(__VA_OPT__(, ) __VA_ARGS__))

#define PHLEX_DETAIL_CREATE_1ARG(token_type, func_name, m)                                         \
  void func_name(token_type<phlex::detail::void_tag> const& m, phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_2ARGS(token_type, func_name, m, cfg)                                   \
  void func_name(token_type<phlex::detail::void_tag> const& m, phlex::configuration const& cfg)

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

#define PHLEX_DETAIL_CREATE_NONTEMPLATE_1ARG(token_type, func_name, m)                             \
  void func_name(token_type const& m, phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_NONTEMPLATE_2ARGS(token_type, func_name, m, cfg)                       \
  void func_name(token_type const& m, phlex::configuration const& cfg)

#define PHLEX_DETAIL_SELECT_NONTEMPLATE_SIGNATURE(token_type, func_name, ...)                      \
  BOOST_PP_IF(BOOST_PP_EQUAL(PHLEX_DETAIL_NARGS(__VA_ARGS__), 1),                                  \
              PHLEX_DETAIL_CREATE_NONTEMPLATE_1ARG,                                                \
              PHLEX_DETAIL_CREATE_NONTEMPLATE_2ARGS)                                               \
  (token_type, func_name, __VA_ARGS__)

#define PHLEX_DETAIL_REGISTER_NONTEMPLATE_PLUGIN(token_type, func_name, dll_alias, ...)            \
  extern "C" PHLEX_DETAIL_SELECT_NONTEMPLATE_SIGNATURE(token_type, dll_alias, __VA_ARGS__)

// ================================================================================================
// Registration macros for source plugins and explicit-provider plugins
//
// Source plugin entry-points use a common opaque carrier so providers and sources retain the same
// loader and exported-symbol shape. We:
//   1. Forward-declare the user's C++ implementation in an internal namespace (takes the proxy
//      by reference). The nested named namespace permits a qualified definition outside it,
//      so the macro need not close a namespace after the user-provided body.
//   2. Define a thin extern "C" shim that accepts registration_carrier by value (matching
//      source_creator_t exactly), constructs the appropriate proxy through the internal factory,
//      and calls the user's implementation.
//   3. Open the user's implementation definition for the body that follows the macro.
#define PHLEX_DETAIL_CREATE_SOURCE_1ARG(token_type, func_name, m)                                  \
  void func_name(token_type<phlex::detail::void_tag> const& m, phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_SOURCE_2ARGS(token_type, func_name, m, cfg)                            \
  void func_name(token_type<phlex::detail::void_tag> const& m, phlex::configuration const& cfg)

#define PHLEX_DETAIL_SELECT_SOURCE_SIGNATURE(token_type, func_name, ...)                           \
  BOOST_PP_IF(BOOST_PP_EQUAL(PHLEX_DETAIL_NARGS(__VA_ARGS__), 1),                                  \
              PHLEX_DETAIL_CREATE_SOURCE_1ARG,                                                     \
              PHLEX_DETAIL_CREATE_SOURCE_2ARGS)                                                    \
  (token_type, func_name, __VA_ARGS__)

#define PHLEX_DETAIL_REGISTER_SOURCE_PLUGIN(token_type, func_name, dll_alias, ...)                 \
  namespace {                                                                                      \
    namespace BOOST_PP_CAT(dll_alias, _detail) {                                                   \
      PHLEX_DETAIL_SELECT_SOURCE_SIGNATURE(token_type, func_name, __VA_ARGS__);                    \
    }                                                                                              \
  }                                                                                                \
  extern "C" void dll_alias(phlex::detail::internal::registration_carrier __phlex_carrier,         \
                            phlex::configuration const& __phlex_config)                            \
  {                                                                                                \
    BOOST_PP_CAT(dll_alias, _detail)::func_name(                                                   \
      phlex::detail::internal::proxy_factory::providers_proxy(__phlex_carrier), __phlex_config);   \
  }                                                                                                \
  PHLEX_DETAIL_SELECT_SOURCE_SIGNATURE(                                                            \
    token_type, BOOST_PP_CAT(dll_alias, _detail)::func_name, __VA_ARGS__)

#define PHLEX_DETAIL_CREATE_NONTEMPLATE_SOURCE_1ARG(token_type, func_name, m)                      \
  void func_name(token_type const& m, phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_NONTEMPLATE_SOURCE_2ARGS(token_type, func_name, m, cfg)                \
  void func_name(token_type const& m, phlex::configuration const& cfg)

#define PHLEX_DETAIL_SELECT_NONTEMPLATE_SOURCE_SIGNATURE(token_type, func_name, ...)               \
  BOOST_PP_IF(BOOST_PP_EQUAL(PHLEX_DETAIL_NARGS(__VA_ARGS__), 1),                                  \
              PHLEX_DETAIL_CREATE_NONTEMPLATE_SOURCE_1ARG,                                         \
              PHLEX_DETAIL_CREATE_NONTEMPLATE_SOURCE_2ARGS)                                        \
  (token_type, func_name, __VA_ARGS__)

#define PHLEX_DETAIL_REGISTER_NONTEMPLATE_SOURCE_PLUGIN(token_type, func_name, dll_alias, ...)     \
  namespace {                                                                                      \
    namespace BOOST_PP_CAT(dll_alias, _detail) {                                                   \
      PHLEX_DETAIL_SELECT_NONTEMPLATE_SOURCE_SIGNATURE(token_type, func_name, __VA_ARGS__);        \
    }                                                                                              \
  }                                                                                                \
  extern "C" void dll_alias(phlex::detail::internal::registration_carrier __phlex_carrier,         \
                            phlex::configuration const& __phlex_config)                            \
  {                                                                                                \
    BOOST_PP_CAT(dll_alias, _detail)::func_name(                                                   \
      phlex::detail::internal::proxy_factory::source_proxy(__phlex_carrier), __phlex_config);      \
  }                                                                                                \
  PHLEX_DETAIL_SELECT_NONTEMPLATE_SOURCE_SIGNATURE(                                                \
    token_type, BOOST_PP_CAT(dll_alias, _detail)::func_name, __VA_ARGS__)

// ================================================================================================
// Driver registration plugin macros
#define PHLEX_DETAIL_CREATE_DRIVER_1ARG(func_name, d)                                              \
  phlex::detail::driver_bundle func_name(phlex::detail::driver_proxy const& d,                     \
                                         phlex::configuration const&)

#define PHLEX_DETAIL_CREATE_DRIVER_2ARGS(func_name, d, cfg)                                        \
  phlex::detail::driver_bundle func_name(phlex::detail::driver_proxy const& d,                     \
                                         phlex::configuration const& cfg)

#define PHLEX_DETAIL_SELECT_DRIVER_SIGNATURE(func_name, ...)                                       \
  BOOST_PP_IF(BOOST_PP_EQUAL(PHLEX_DETAIL_NARGS(__VA_ARGS__), 1),                                  \
              PHLEX_DETAIL_CREATE_DRIVER_1ARG,                                                     \
              PHLEX_DETAIL_CREATE_DRIVER_2ARGS)                                                    \
  (func_name, __VA_ARGS__)

// The driver entry-point cannot use extern "C" directly because driver_bundle is a C++ type.
// Instead we forward-declare the user's C++ implementation in an internal namespace, define a thin
// extern "C" shim that writes the result through an out-parameter (which has a void return type,
// compatible with C linkage), and then open the qualified implementation definition for the body
// that follows.
#define PHLEX_DETAIL_REGISTER_DRIVER_PLUGIN(func_name, dll_alias, ...)                             \
  namespace {                                                                                      \
    namespace BOOST_PP_CAT(dll_alias, _detail) {                                                   \
      PHLEX_DETAIL_SELECT_DRIVER_SIGNATURE(func_name, __VA_ARGS__);                                \
    }                                                                                              \
  }                                                                                                \
  extern "C" void dll_alias(phlex::detail::driver_proxy const& __phlex_proxy,                      \
                            phlex::configuration const& __phlex_config,                            \
                            phlex::detail::driver_bundle* __phlex_out)                             \
  {                                                                                                \
    *__phlex_out = BOOST_PP_CAT(dll_alias, _detail)::func_name(__phlex_proxy, __phlex_config);     \
  }                                                                                                \
  PHLEX_DETAIL_SELECT_DRIVER_SIGNATURE(BOOST_PP_CAT(dll_alias, _detail)::func_name, __VA_ARGS__)
// NOLINTEND(bugprone-macro-parentheses)

#endif // PHLEX_DETAIL_PLUGIN_MACROS_HPP
