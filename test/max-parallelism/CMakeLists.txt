include(NProc)

add_library(check_parallelism MODULE check_parallelism.cpp)
target_link_libraries(check_parallelism PRIVATE Boost::json phlex::core)

configure_file(
  check_parallelism_default.jsonnet.in check_parallelism_default.jsonnet @ONLY
  )
add_test(NAME job:check_parallelism_default
         COMMAND phlex -c
                 ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_default.jsonnet
         )

configure_file(
  check_parallelism_cli.jsonnet check_parallelism_cli.jsonnet COPYONLY
  )
add_test(NAME job:check_parallelism_cli
         COMMAND phlex -c
                 ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_cli.jsonnet -j 7
         )

configure_file(
  check_parallelism_config.jsonnet check_parallelism_config.jsonnet COPYONLY
  )
add_test(NAME job:check_parallelism_config
         COMMAND phlex -c
                 ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_config.jsonnet
         )

configure_file(
  check_parallelism_cli_over_config.jsonnet
  check_parallelism_cli_over_config.jsonnet COPYONLY
  )
add_test(
  NAME job:check_parallelism_cli_over_config
  COMMAND
    phlex -c
    ${CMAKE_CURRENT_BINARY_DIR}/check_parallelism_cli_over_config.jsonnet -j 9
  )

set_tests_properties(
  job:check_parallelism_default job:check_parallelism_cli
  job:check_parallelism_config job:check_parallelism_cli_over_config
  PROPERTIES ENVIRONMENT "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}"
  )

