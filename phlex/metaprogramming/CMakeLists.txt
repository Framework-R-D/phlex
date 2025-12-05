# Interface library
add_library(phlex_metaprogramming_int INTERFACE)
target_include_directories(
  phlex_metaprogramming_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                                      "$<INSTALL_INTERFACE:include>"
  )

target_link_libraries(phlex_metaprogramming_int INTERFACE Boost::boost)
target_sources(
  phlex_metaprogramming_int
  INTERFACE FILE_SET HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}
            FILES delegate.hpp type_deduction.hpp
  )

add_library(phlex::metaprogramming ALIAS phlex_metaprogramming_int)

install(TARGETS phlex_metaprogramming_int EXPORT phlex FILE_SET HEADERS)
