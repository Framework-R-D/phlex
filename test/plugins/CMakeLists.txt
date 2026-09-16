add_library(module MODULE module.cpp)
target_link_libraries(module PRIVATE phlex::module)

add_library(ij_source MODULE ij_source.cpp)
target_link_libraries(ij_source PRIVATE phlex::source)

add_library(output MODULE output.cpp)
target_link_libraries(output PRIVATE phlex::module)

add_library(register_resources_for_testing MODULE register_resources_for_testing.cpp)
target_link_libraries(register_resources_for_testing PRIVATE phlex::resource)

add_library(verify_resources MODULE verify_resources.cpp)
target_link_libraries(verify_resources PRIVATE phlex::module)

cet_test(
  job:add
  HANDBUILT
  TEST_EXEC
  phlex::phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/add.jsonnet
  --stage test
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
  ${CMAKE_CURRENT_SOURCE_DIR}/test_resources.jsonnet
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}"
)
