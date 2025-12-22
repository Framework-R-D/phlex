#ifndef PHLEX_DETAIL_PLUGIN_MACROS_HPP
#define PHLEX_DETAIL_PLUGIN_MACROS_HPP

#include "boost/preprocessor.hpp"

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

#define PHLEX_DETAIL_REGISTER_PLUGIN(token_type, func_name, dll_alias, ...)                        \
  static PHLEX_DETAIL_SELECT_SIGNATURE(token_type, func_name, __VA_ARGS__);                        \
  BOOST_DLL_ALIAS(func_name, dll_alias)                                                            \
  PHLEX_DETAIL_SELECT_SIGNATURE(token_type, func_name, __VA_ARGS__)

#endif // PHLEX_DETAIL_PLUGIN_MACROS_HPP
