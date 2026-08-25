add_definitions("-UNDEBUG")

find_package(Catch2 REQUIRED)

# Suppress Catch2's use of __COUNTER__ (a compiler extension, not standard C++)
# by propagating CATCH_CONFIG_NO_COUNTER to all consumers of Catch2::Catch2,
# regardless of whether Catch2 was resolved via find_package or FetchContent.
target_compile_definitions(Catch2::Catch2 INTERFACE CATCH_CONFIG_NO_COUNTER)

include(CetTest)

cet_test_env(SPDLOG_LEVEL=debug)

cet_test(accumulator USE_CATCH2_MAIN SOURCE accumulator_test.cpp LIBRARIES phlex::core_internal)
cet_test(concepts SOURCE concepts_test.cpp LIBRARIES phlex::core_internal)
cet_test(
  adjust_config
  USE_CATCH2_MAIN
  SOURCE
  adjust_config_test.cpp
  LIBRARIES
  run_phlex_internal
  Boost::json
)
cet_test(config USE_CATCH2_MAIN SOURCE config_test.cpp LIBRARIES
         phlex::configuration
)
cet_test(type_deduction SOURCE type_deduction_test.cpp LIBRARIES
         phlex::metaprogramming
)
cet_test(type_id USE_CATCH2_MAIN SOURCE type_id_test.cpp LIBRARIES phlex::model_internal fmt::fmt)
cet_test(layer_path USE_CATCH2_MAIN SOURCE layer_path_test.cpp LIBRARIES phlex::model_internal fmt::fmt)
cet_test(identifier USE_CATCH2_MAIN SOURCE identifier_test.cpp LIBRARIES phlex::model phlex::configuration Boost::json)
cet_test(
  yielding_driver
  USE_CATCH2_MAIN
  SOURCE
  yielding_driver_test.cpp
  LIBRARIES
  phlex::core_internal
  TBB::tbb
)

cet_test(
  allowed_families
  USE_CATCH2_MAIN
  SOURCE
  allowed_families_test.cpp
  LIBRARIES
  phlex::core_internal
  Boost::json
  layer_generator_internal
)
cet_test(
  vector_of_abstract_types
  USE_CATCH2_MAIN
  SOURCE
  vector_of_abstract_types_test.cpp
  LIBRARIES
  phlex::core_internal
  layer_generator_internal
)
cet_test(
  cached_execution
  USE_CATCH2_MAIN
  SOURCE
  cached_execution_test.cpp
  LIBRARIES
  phlex::core_internal
  Boost::json
  layer_generator_internal
)
cet_test(
  class_registration
  USE_CATCH2_MAIN
  SOURCE
  class_registration_test.cpp
  LIBRARIES
  phlex::core_internal
  Boost::json
)

cet_test(
  fold_duplicate_layer_name
  USE_CATCH2_MAIN
  SOURCE
  fold_duplicate_layer_name_test.cpp
  LIBRARIES
  phlex::core_internal
  spdlog::spdlog
  layer_generator_internal
)

cet_test(filter_impl USE_CATCH2_MAIN SOURCE filter_impl_test.cpp LIBRARIES
         phlex::core_internal
)
cet_test(
  filter
  USE_CATCH2_MAIN
  SOURCE
  filter_test.cpp
  LIBRARIES
  phlex::core_internal
  Boost::json
  layer_generator_internal
)
cet_test(
  framework_graph
  USE_CATCH2_MAIN
  SOURCE
  framework_graph_test.cpp
  LIBRARIES
  phlex::core_internal
  Boost::json
  layer_generator_internal
)
cet_test(
  function_registration
  USE_CATCH2_MAIN
  SOURCE
  function_registration_test.cpp
  LIBRARIES
  phlex::core_internal
  Boost::json
)
cet_test(
  hierarchical_nodes
  USE_CATCH2_MAIN
  SOURCE
  hierarchical_nodes_test.cpp
  LIBRARIES
  Boost::json
  TBB::tbb
  phlex::core_internal
  spdlog::spdlog
  layer_generator_internal
)
cet_test(
  layer_generator_unit
  USE_CATCH2_MAIN
  SOURCE
  layer_generator_test.cpp
  LIBRARIES
  phlex::core_internal
  layer_generator_internal
)
cet_test(
  multiple_function_registration
  USE_CATCH2_MAIN
  SOURCE
  multiple_function_registration_test.cpp
  LIBRARIES
  Boost::json
  phlex::core_internal
)
cet_test(
  output_products
  USE_CATCH2_MAIN
  SOURCE output_products_test.cpp
  LIBRARIES layer_generator_internal phlex::core_internal spdlog::spdlog
)
cet_test(
  data_layer_hierarchy
  USE_CATCH2_MAIN
  SOURCE
  data_layer_hierarchy_test.cpp
  LIBRARIES
  phlex::model_internal
  phlex::utilities_internal
)
cet_test(
  data_product_concept
  USE_CATCH2_MAIN
  SOURCE
  data_product_concept_test.cpp
  LIBRARIES
  phlex::model
)
cet_test(
  data_product_concept_registry
  USE_CATCH2_MAIN
  SOURCE
  data_product_concept_registry_test.cpp
  LIBRARIES
  phlex::model
  TBB::tbb
)
cet_test(data_cell_index USE_CATCH2_MAIN SOURCE data_cell_index_test.cpp LIBRARIES
         phlex::model_internal
)
cet_test(index_generator USE_CATCH2_MAIN SOURCE index_generator_test.cpp LIBRARIES
         phlex::model_internal
)
cet_test(fixed_hierarchy USE_CATCH2_MAIN SOURCE fixed_hierarchy_test.cpp LIBRARIES
         phlex::model
)
cet_test(data_cell_tracker USE_CATCH2_MAIN SOURCE data_cell_tracker_test.cpp LIBRARIES
         phlex::model
         TBB::tbb
)
cet_test(flush_gate USE_CATCH2_MAIN SOURCE flush_gate_test.cpp LIBRARIES
         phlex::model_internal
         TBB::tbb
)
cet_test(product_handle USE_CATCH2_MAIN SOURCE product_handle_test.cpp LIBRARIES
         phlex::core_internal
)
cet_test(product_store USE_CATCH2_MAIN SOURCE product_store_test.cpp LIBRARIES
         phlex::core_internal
)
cet_test(products USE_CATCH2_MAIN SOURCE products_test.cpp LIBRARIES
         phlex::model_internal
)
cet_test(
  fold
  USE_CATCH2_MAIN
  SOURCE
  fold_test.cpp
  LIBRARIES
  phlex::core_internal
  spdlog::spdlog
  layer_generator_internal
)
cet_test(
  repeater_node
  USE_CATCH2_MAIN
  SOURCE
  repeater_node_test.cpp
  LIBRARIES
  phlex::core_internal
)
cet_test(
  transform_node
  USE_CATCH2_MAIN
  SOURCE
  transform_node_test.cpp
  LIBRARIES
  phlex::core_internal
)
cet_test(
  multilayer_join
  USE_CATCH2_MAIN
  SOURCE
  multilayer_join_test.cpp
  LIBRARIES
  phlex::core_internal
)
cet_test(
  replicated
  USE_CATCH2_MAIN
  SOURCE
  replicated_test.cpp
  LIBRARIES
  TBB::tbb
  phlex::utilities_internal
  spdlog::spdlog
)
cet_test(
  product_selecting
  USE_CATCH2_MAIN
  SOURCE
  product_selecting_test.cpp
  LIBRARIES
  phlex::core
  layer_generator
  Boost::json
  fmt::fmt
)
cet_test(product_selector USE_CATCH2_MAIN SOURCE product_selector_test.cpp LIBRARIES
         phlex::core_internal
)
cet_test(core_misc USE_CATCH2_MAIN SOURCE core_misc_test.cpp LIBRARIES
         phlex::core_internal
         Boost::json
)
cet_test(
  provider
  USE_CATCH2_MAIN
  SOURCE
  provider_test.cpp
  LIBRARIES
  phlex::core_internal
  layer_generator_internal
)
cet_test(
  type_distinction
  USE_CATCH2_MAIN
  SOURCE
  type_distinction_test.cpp
  LIBRARIES
  spdlog::spdlog
  phlex::core_internal
  layer_generator_internal
)
cet_test(
  unfold
  USE_CATCH2_MAIN
  SOURCE
  unfold_test.cpp
  LIBRARIES
  Boost::json
  phlex::core_internal
  TBB::tbb
  spdlog::spdlog
  layer_generator_internal
)

add_subdirectory(benchmarks)

add_subdirectory(max-parallelism)
add_subdirectory(memory-checks)
add_subdirectory(plugins)
add_subdirectory(utilities)
add_subdirectory(mock-workflow)
add_subdirectory(demo-giantdata)
add_subdirectory(python)
add_subdirectory(tbb-preview)

if(PHLEX_USE_FORM)
  add_subdirectory(form)
endif()
