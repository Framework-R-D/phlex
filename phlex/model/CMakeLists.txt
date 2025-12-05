add_library(
  phlex_model SHARED
  algorithm_name.cpp
  data_cell_counter.cpp
  data_layer_hierarchy.cpp
  data_cell_index.cpp
  product_matcher.cpp
  product_store.cpp
  products.cpp
  product_specification.cpp
  )
target_sources(
  phlex_model
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${PROJECT_SOURCE_DIR}
         FILES
         algorithm_name.hpp
         fwd.hpp
         handle.hpp
         data_cell_counter.hpp
         data_layer_hierarchy.hpp
         data_cell_index.hpp
         product_matcher.hpp
         product_specification.hpp
         product_store.hpp
         products.hpp
         type_id.hpp
  )
target_include_directories(phlex_model PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
  phlex_model
  PUBLIC Boost::boost spdlog::spdlog fmt::fmt
  PRIVATE phlex::graph phlex::utilities
  )

# Interface library
add_library(phlex_model_int INTERFACE)
target_include_directories(
  phlex_model_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                            "$<INSTALL_INTERFACE:include>"
  )
target_link_libraries(
  phlex_model_int INTERFACE phlex_model spdlog::spdlog TBB::tbb
  )

add_library(phlex::model ALIAS phlex_model_int)

install(TARGETS phlex_model phlex_model_int EXPORT phlex FILE_SET HEADERS)
