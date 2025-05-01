add_library(
  phlex_model SHARED
  algorithm_name.cpp
  level_counter.cpp
  level_hierarchy.cpp
  level_id.cpp
  product_matcher.cpp
  product_store.cpp
  products.cpp
  qualified_name.cpp
  )
target_include_directories(phlex_model PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
  phlex_model
  PUBLIC Boost::boost spdlog::spdlog
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

install(TARGETS phlex_model)
