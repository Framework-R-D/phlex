add_library(timed_busy SHARED timed_busy.cpp)
target_include_directories(timed_busy PRIVATE ${PROJECT_SOURCE_DIR})

add_library(algorithm INTERFACE)
target_include_directories(algorithm INTERFACE ${PROJECT_SOURCE_DIR})
target_link_libraries(
  algorithm INTERFACE timed_busy phlex::module spdlog::spdlog
  )

add_library(mock_workflow_source MODULE source.cpp)
target_link_libraries(mock_workflow_source PRIVATE timed_busy phlex::module)

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
set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.d)
file(MAKE_DIRECTORY ${TEST_DIR})
add_test(
  NAME ${TEST_NAME}
  COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.jsonnet
  WORKING_DIRECTORY ${TEST_DIR}
  )
set_tests_properties(
  ${TEST_NAME} PROPERTIES ENVIRONMENT
                          PHLEX_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR}
  )
