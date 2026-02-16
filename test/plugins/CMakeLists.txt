add_library(module MODULE module.cpp)
target_link_libraries(module PRIVATE phlex::module)

add_library(ij_source MODULE ij_source.cpp)
target_link_libraries(ij_source PRIVATE phlex::module)

add_library(output MODULE output.cpp)
target_link_libraries(output PRIVATE phlex::module)

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
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}"
)
