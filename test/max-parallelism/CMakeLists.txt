include(NProc)

add_library(check_parallelism MODULE check_parallelism.cpp)
target_link_libraries(check_parallelism PRIVATE Boost::json phlex::core)

add_library(provide_parallelism MODULE provide_parallelism.cpp)
target_link_libraries(provide_parallelism PRIVATE phlex::core)

configure_file(
    check_parallelism_default.jsonnet.in
    check_parallelism_default.jsonnet
    @ONLY
)
cet_test(
  job:check_parallelism_default
  HANDBUILT
  TEST_EXEC
  phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_default.jsonnet
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}"
)

configure_file(
    check_parallelism_cli.jsonnet
    check_parallelism_cli.jsonnet
    COPYONLY
)
cet_test(
  job:check_parallelism_cli
  HANDBUILT
  TEST_EXEC
  phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_cli.jsonnet
  -j
  7
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}"
)

configure_file(
    check_parallelism_config.jsonnet
    check_parallelism_config.jsonnet
    COPYONLY
)
cet_test(
  job:check_parallelism_config
  HANDBUILT
  TEST_EXEC
  phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_config.jsonnet
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}"
)

configure_file(
    check_parallelism_cli_over_config.jsonnet
    check_parallelism_cli_over_config.jsonnet
    COPYONLY
)
cet_test(
  job:check_parallelism_cli_over_config
  HANDBUILT
  TEST_EXEC
  phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_cli_over_config.jsonnet
  -j
  9
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}"
)
