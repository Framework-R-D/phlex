add_definitions("-UNDEBUG")

find_package(Catch2 REQUIRED)

function(add_unit_test BASENAME)
  set(multiValueArgs LIBRARIES)
  cmake_parse_arguments(UT "" "" "${multiValueArgs}" ${ARGN})
  add_executable(${BASENAME} ${BASENAME}.cpp)
  target_link_libraries(${BASENAME} PRIVATE ${UT_LIBRARIES})
  set_target_properties(
    ${BASENAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                           ${CMAKE_CURRENT_BINARY_DIR}/bin
    )
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.d)
  add_test(
    NAME ${BASENAME}
    COMMAND ${BASENAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.d
    )
  set_tests_properties(${BASENAME} PROPERTIES ENVIRONMENT SPDLOG_LEVEL=debug)
endfunction()

function(add_catch_test BASENAME)
  set(multiValueArgs LIBRARIES)
  cmake_parse_arguments(UT "${optionalArgs}" "" "${multiValueArgs}" ${ARGN})
  add_unit_test(${BASENAME} LIBRARIES Catch2::Catch2WithMain ${UT_LIBRARIES})
endfunction()

add_unit_test(concepts LIBRARIES phlex::core)
add_unit_test(string_literal LIBRARIES phlex::utilities)
add_unit_test(type_deduction LIBRARIES phlex::metaprogramming)
add_unit_test(type_id LIBRARIES phlex::model fmt::fmt)
add_unit_test(yielding_driver LIBRARIES phlex::core TBB::tbb)

add_catch_test(allowed_families LIBRARIES phlex::core Boost::json)
add_catch_test(vector_of_abstract_types LIBRARIES phlex::core)
add_catch_test(cached_execution LIBRARIES phlex::core Boost::json)
add_catch_test(cached_product_stores LIBRARIES phlex::core)
add_catch_test(class_registration LIBRARIES phlex::core Boost::json)
add_catch_test(different_hierarchies LIBRARIES phlex::core)
add_catch_test(filter_impl LIBRARIES phlex::core)
add_catch_test(filter LIBRARIES phlex::core Boost::json)
add_catch_test(function_registration LIBRARIES phlex::core Boost::json)
add_catch_test(hierarchical_nodes LIBRARIES Boost::json TBB::tbb phlex::core)
add_catch_test(multiple_function_registration LIBRARIES Boost::json phlex::core)
add_catch_test(level_counting LIBRARIES phlex::model phlex::utilities)
add_catch_test(level_id LIBRARIES phlex::model)
add_catch_test(product_handle LIBRARIES phlex::core)
add_catch_test(product_matcher LIBRARIES phlex::model)
add_catch_test(product_store LIBRARIES phlex::core)
add_catch_test(fold LIBRARIES phlex::core)
add_catch_test(replicated LIBRARIES TBB::tbb phlex::utilities spdlog::spdlog)
add_catch_test(serializer LIBRARIES phlex::core TBB::tbb)
add_catch_test(specified_label LIBRARIES phlex::core)
add_catch_test(unfold LIBRARIES Boost::json phlex::core TBB::tbb)

add_subdirectory(benchmarks)
add_subdirectory(max-parallelism)
add_subdirectory(memory-checks)
add_subdirectory(plugins)
add_subdirectory(utilities)
add_subdirectory(mock-workflow)
add_subdirectory(demo-giantdata)
add_subdirectory(python)

if(PHLEX_USE_FORM)
  add_subdirectory(form)
endif()
