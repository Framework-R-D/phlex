add_library(timed_busy SHARED timed_busy.cpp)
target_include_directories(timed_busy PRIVATE ${PROJECT_SOURCE_DIR})

add_library(algorithm INTERFACE)
target_include_directories(algorithm INTERFACE ${PROJECT_SOURCE_DIR})
target_link_libraries(
    algorithm
    INTERFACE timed_busy phlex::module spdlog::spdlog
)

add_library(id_provider MODULE id_provider.cpp)
target_link_libraries(id_provider PRIVATE phlex::module)

function(add_module NAME)
    add_library(${NAME} MODULE ${NAME}.cpp)
    target_link_libraries(${NAME} PRIVATE algorithm)
endfunction()

add_module(ion_and_scint)
add_module(pd_fast_sim)
add_module(MC_truth_algorithm)
add_module(largeant)
add_module(three_tuple_algorithm)

set(TEST_NAME mock-workflow)
cet_test(
  ${TEST_NAME}
  HANDBUILT
  TEST_EXEC
  phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.jsonnet
  TEST_PROPERTIES
  ENVIRONMENT
  PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}
)
