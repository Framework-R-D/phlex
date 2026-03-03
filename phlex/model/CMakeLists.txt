cet_make_library(
  LIBRARY_NAME
  phlex_model
  SHARED
  SOURCE
  algorithm_name.cpp
  data_cell_counter.cpp
  data_layer_hierarchy.cpp
  data_cell_index.cpp
  identifier.cpp
  product_matcher.cpp
  product_store.cpp
  products.cpp
  product_specification.cpp
  LIBRARIES
  PUBLIC
  Boost::boost
  spdlog::spdlog
  fmt::fmt
  PRIVATE
  phlex::utilities
  TBB::tbb
)

install(
  FILES
    algorithm_name.hpp
    fwd.hpp
    handle.hpp
    data_cell_counter.hpp
    data_layer_hierarchy.hpp
    data_cell_index.hpp
    identifier.hpp
    product_matcher.hpp
    product_specification.hpp
    product_store.hpp
    products.hpp
    type_id.hpp
  DESTINATION include/phlex/model
)

target_include_directories(phlex_model PRIVATE ${PROJECT_SOURCE_DIR})

# Interface library
cet_make_library(
  LIBRARY_NAME
  phlex_model_int
  EXPORT_NAME
  model
  INTERFACE
  NO_SOURCE
  LIBRARIES
  phlex_model
  spdlog::spdlog
  TBB::tbb
)
