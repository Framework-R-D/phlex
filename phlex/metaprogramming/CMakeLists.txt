# Interface library
add_library(phlex_metaprogramming_int INTERFACE)
target_include_directories(
  phlex_metaprogramming_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                                      "$<INSTALL_INTERFACE:include>"
  )

target_link_libraries(phlex_metaprogramming_int INTERFACE Boost::boost)

add_library(phlex::metaprogramming ALIAS phlex_metaprogramming_int)

install(TARGETS phlex_metaprogramming_int)
