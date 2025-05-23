add_library(fibonacci_numbers SHARED fibonacci_numbers.cpp)
target_include_directories(fibonacci_numbers PRIVATE ${PROJECT_SOURCE_DIR})

add_library(benchmarks_source MODULE benchmarks_source.cpp)
target_link_libraries(benchmarks_source PRIVATE phlex::model Boost::json)

add_library(last_index MODULE last_index.cpp)
target_link_libraries(last_index PRIVATE phlex::module)

add_library(read_id MODULE read_id.cpp)
target_link_libraries(read_id PRIVATE phlex::module)

add_library(read_index MODULE read_index.cpp)
target_link_libraries(read_index PRIVATE phlex::module)

add_library(plus_one MODULE plus_one.cpp)
target_link_libraries(plus_one PRIVATE phlex::module)

add_library(plus_101 MODULE plus_101.cpp)
target_link_libraries(plus_101 PRIVATE phlex::module)

add_library(accept_even_ids MODULE accept_even_ids.cpp)
target_link_libraries(accept_even_ids PRIVATE phlex::module)

add_library(accept_even_numbers MODULE accept_even_numbers.cpp)
target_link_libraries(accept_even_numbers PRIVATE phlex::module)

add_library(accept_fibonacci_numbers MODULE accept_fibonacci_numbers.cpp)
target_link_libraries(
  accept_fibonacci_numbers PRIVATE phlex::module fibonacci_numbers
  )

add_library(
  verify_even_fibonacci_numbers MODULE verify_even_fibonacci_numbers.cpp
  )
target_link_libraries(
  verify_even_fibonacci_numbers PRIVATE phlex::module fibonacci_numbers
  )

add_library(verify_difference MODULE verify_difference.cpp)
target_link_libraries(verify_difference PRIVATE phlex::module)

foreach (
  I IN
  ITEMS 01
        02
        03
        04
        05
        06
        07
        08
        09
  )
  set(test_name benchmark:${I})
  set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/benchmark-${I}.d)
  file(MAKE_DIRECTORY ${TEST_DIR})
  add_test(
    NAME ${test_name}
    COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/benchmark-${I}.jsonnet -g
            benchmark-${I}
    WORKING_DIRECTORY ${TEST_DIR}
    )
  set_tests_properties(
    ${test_name} PROPERTIES ENVIRONMENT
                            PHLEX_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR}
    )
  dot_test(benchmark-${I})
endforeach ()
