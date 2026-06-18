cet_make_library(
  LIBRARY_NAME
  phlex_model
  SHARED
  SOURCE
  algorithm_name.cpp
  data_cell_counts.cpp
  data_cell_tracker.cpp
  flush_gate.cpp
  fixed_hierarchy.cpp
  data_layer_hierarchy.cpp
  data_cell_index.cpp
  identifier.cpp
  layer_path.cpp
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
    index_generator.hpp
    fixed_hierarchy.hpp
    flush_messages.hpp
    fwd.hpp
    handle.hpp
    data_cell_counts.hpp
    data_cell_tracker.hpp
    flush_gate.hpp
    data_layer_hierarchy.hpp
    data_cell_index.hpp
    identifier.hpp
    layer_path.hpp
    product_matcher.hpp
    product_specification.hpp
    product_store.hpp
    products.hpp
    type_id.hpp
  DESTINATION include/phlex/model
)

target_include_directories(phlex_model PRIVATE ${PROJECT_SOURCE_DIR})
phlex_apply_symbol_visibility(phlex_model)
phlex_apply_optimizations(phlex_model)

phlex_make_internal_library(
  phlex_model
  LIBRARIES
  PUBLIC Boost::boost spdlog::spdlog fmt::fmt
  PRIVATE phlex_utilities_internal TBB::tbb
)
add_library(phlex::model_internal ALIAS phlex_model_internal)

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
