# Interface library
add_library(phlex_metaprogramming_int INTERFACE)
target_include_directories(
  phlex_metaprogramming_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                                      "$<INSTALL_INTERFACE:include>"
  )

add_library(phlex::metaprogramming ALIAS phlex_metaprogramming_int)
