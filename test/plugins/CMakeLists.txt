add_library(module MODULE module.cpp)
target_link_libraries(module PRIVATE phlex::module)

add_library(output MODULE output.cpp)
target_link_libraries(output PRIVATE phlex::module)

add_library(source MODULE source.cpp)
target_link_libraries(source PRIVATE Boost::json phlex::core)

cet_test(
  job:add
  HANDBUILT
  TEST_EXEC
  phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/add.jsonnet
  TEST_PROPERTIES
  ENVIRONMENT
  "SPDLOG_LEVEL=debug;PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}"
  )
