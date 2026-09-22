add_library(module MODULE module.cpp)
target_link_libraries(module PRIVATE phlex::module)

add_library(ij_source MODULE ij_source.cpp)
target_link_libraries(ij_source PRIVATE phlex::source)

add_library(output MODULE output.cpp)
target_link_libraries(output PRIVATE phlex::module)

add_library(test_resources MODULE resources.cpp)
target_link_libraries(test_resources PRIVATE phlex::resource)

add_library(resources_module MODULE resources_module.cpp)
target_link_libraries(resources_module PRIVATE phlex::module)

cet_test(
  job:add
  HANDBUILT
  TEST_EXEC
  phlex::phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/add.jsonnet
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}"
)

cet_test(
  job:resources
  HANDBUILT
  TEST_EXEC
  phlex::phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/resources.jsonnet
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}"
)
