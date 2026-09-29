# Copyright (C) 2025 ...

if(FORM_USE_ROOT_STORAGE)
  add_subdirectory(data_products)

  find_package(ROOT REQUIRED COMPONENTS Core)
  add_library(form_test_utils INTERFACE)
  target_link_libraries(form_test_utils INTERFACE ROOT::Core)
  target_include_directories(form_test_utils INTERFACE ${PROJECT_SOURCE_DIR})

  list(APPEND FORM_TEST_TECHNOLOGIES "ROOT_TTREE")
  if(FORM_USE_RNTUPLE_STORAGE)
    list(APPEND FORM_TEST_TECHNOLOGIES "ROOT_RNTUPLE")
  endif()

  foreach(TECH IN LISTS FORM_TEST_TECHNOLOGIES)
    cet_test(
        "WriteVector_${TECH}"
        SOURCE
        writer.cpp
        toy_tracker.cpp
        LIBRARIES
        form
        form_test_data_products
        form_test_utils
        TEST_ARGS
        "${CMAKE_CURRENT_BINARY_DIR}/toy_${TECH}.root"
        "${CMAKE_CURRENT_BINARY_DIR}/toy_checksums_${TECH}.txt"
        ${TECH}
    )
    target_include_directories("WriteVector_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)

    cet_test(
        "ReadVector_${TECH}"
        SOURCE
        reader.cpp
        LIBRARIES
        form
        form_test_data_products
        form_test_utils
        TEST_ARGS
        "${CMAKE_CURRENT_BINARY_DIR}/toy_${TECH}.root"
        "${CMAKE_CURRENT_BINARY_DIR}/toy_checksums_${TECH}.txt"
        ${TECH}
        TEST_PROPERTIES
        DEPENDS
        "WriteVector_${TECH}"
    )
    target_include_directories("ReadVector_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)

    cet_test(
        "form_navigation_${TECH}"
        SOURCE
        form_navigation_test.cpp
        LIBRARIES
        storage
        root_storage
        TEST_ARGS
        "${CMAKE_CURRENT_BINARY_DIR}/toy_${TECH}.root"
        ${TECH}
        TEST_PROPERTIES
        DEPENDS
        "WriteVector_${TECH}"
    )
    target_include_directories("form_navigation_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)

    # DUNE-shaped navigation example covering multiple hierarchies and creators.
    cet_test(
        "dune_example_write_navigation_${TECH}"
        SOURCE
        dune_example_writer.cpp
        dune_example_hit_maker.cpp
        LIBRARIES
        form
        form_test_dune_example_data_products
        form_test_utils
        TEST_ARGS
        "${CMAKE_CURRENT_BINARY_DIR}/dune_example_navigation_${TECH}.root"
        ${TECH}
    )
    target_include_directories(
      "dune_example_write_navigation_${TECH}"
      PRIVATE ${PROJECT_SOURCE_DIR}/form
    )

    cet_test(
        "dune_example_navigation_dump_${TECH}"
        SOURCE
        dune_example_navigation_dump.cpp
        dune_example_hit_maker.cpp
        LIBRARIES
        storage
        root_storage
        TEST_ARGS
        "${CMAKE_CURRENT_BINARY_DIR}/dune_example_navigation_${TECH}.root"
        ${TECH}
        TEST_PROPERTIES
        DEPENDS
        "dune_example_write_navigation_${TECH}"
    )
    target_include_directories(
      "dune_example_navigation_dump_${TECH}"
      PRIVATE ${PROJECT_SOURCE_DIR}/form
    )

    cet_test(
        "form_root_schema_write_${TECH}"
        SOURCE
        form_root_schema_write_test.cpp
        toy_tracker.cpp
        LIBRARIES
        form
        root_storage
        form_test_data_products
        form_test_utils
        TEST_WORKDIR
        "form_root_schema_test_${TECH}"
        TEST_ARGS
        ${TECH}
    )
    target_include_directories("form_root_schema_write_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)

    cet_test(
       "form_root_schema_read_${TECH}"
       SOURCE
       form_root_schema_read_test.cpp
       LIBRARIES
       form
       root_storage
       form_test_data_products_schema_evolution
       form_test_utils
       DIRTY_WORKDIR
       TEST_WORKDIR
       "form_root_schema_test_${TECH}"
       REQUIRED_FIXTURES
       "form_root_schema_write_${TECH}"
       TEST_ARGS
       ${TECH}
    )
    target_include_directories("form_root_schema_read_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)

    cet_test(
      "check_root_schema_evolution_${TECH}"
      HANDBUILT
      TEST_EXEC
      ${CMAKE_COMMAND}
      TEST_ARGS
      -E
      compare_files
      "form_root_schema_write_log_${TECH}.txt"
      "form_root_schema_read_log_${TECH}.txt"
      DIRTY_WORKDIR
      TEST_WORKDIR
      "form_root_schema_test_${TECH}"
      REQUIRED_FIXTURES
      "form_root_schema_write_${TECH}"
      "form_root_schema_read_${TECH}"
    )

    cet_test("form_storage_${TECH}" USE_CATCH2_MAIN SOURCE form_storage_test.cpp LIBRARIES
             root_storage
             storage
             persistence
             form
             form_test_utils
             TEST_ARGS
             --technology
             ${TECH}
             TEST_PROPERTIES
             RESOURCE_LOCK
             "FORMTestFile.root"
    )
    target_include_directories("form_storage_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)
  endforeach()
endif()

cet_test(
  job:form_module
  HANDBUILT
  TEST_EXEC
  phlex::phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/form_test.jsonnet
  --stage test
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
)

if(FORM_USE_ROOT_STORAGE AND FORM_USE_RNTUPLE_STORAGE)
  cet_test(
    job:form_module_rntuple
    HANDBUILT
    TEST_EXEC
    phlex::phlex
    TEST_ARGS
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/form_test_rntuple.jsonnet
    --stage test
    TEST_PROPERTIES
    ENVIRONMENT
    "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
  )
endif()

# form_module output configurations, exercised on the DUNE hit example. The jobs take their stage
# from dune_example_hits.libsonnet, so each also runs as 'phlex -c <jsonnet>'.
if(FORM_USE_ROOT_STORAGE)
  add_library(dune_example_hits MODULE dune_example_hits.cpp dune_example_hit_maker.cpp)
  target_link_libraries(
    dune_example_hits
    PRIVATE phlex::module form_test_dune_example_data_products
  )

  # Checks each (file, technology) output of a job and reads its products back.
  add_executable(
    form_multiple_outputs_check
    form_multiple_outputs_check.cpp
    dune_example_hit_maker.cpp
  )
  target_link_libraries(
    form_multiple_outputs_check
    PRIVATE storage root_storage form_test_dune_example_data_products
  )
  target_include_directories(form_multiple_outputs_check PRIVATE ${PROJECT_SOURCE_DIR}/form)

  # Single-output keys.
  cet_test(
    job:form_dune_outputs_legacy
    HANDBUILT
    TEST_EXEC
    phlex::phlex
    TEST_ARGS
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/form_dune_outputs_legacy.jsonnet
    TEST_WORKDIR
    form_dune_outputs_legacy
    TEST_PROPERTIES
    ENVIRONMENT
    "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
  )
  cet_test(
    form_dune_outputs_legacy_check
    HANDBUILT
    TEST_EXEC
    form_multiple_outputs_check
    TEST_ARGS
    test
    dune_legacy.root
    ROOT_TTREE
    hitCandidates,wireCandidates,spillCandidates
    DIRTY_WORKDIR
    TEST_WORKDIR
    form_dune_outputs_legacy
    REQUIRED_FIXTURES
    job:form_dune_outputs_legacy
  )

  # Multiple TTree output files, including a product written to both files.
  cet_test(
    job:form_dune_outputs_multiple_files
    HANDBUILT
    TEST_EXEC
    phlex::phlex
    TEST_ARGS
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/form_dune_outputs_multiple_files.jsonnet
    TEST_WORKDIR
    form_dune_outputs_multiple_files
    TEST_PROPERTIES
    ENVIRONMENT
    "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
  )
  cet_test(
    form_dune_outputs_multiple_files_check
    HANDBUILT
    TEST_EXEC
    form_multiple_outputs_check
    TEST_ARGS
    test
    dune_roi.root
    ROOT_TTREE
    hitCandidates,wireCandidates
    dune_summary.root
    ROOT_TTREE
    wireCandidates,spillCandidates
    DIRTY_WORKDIR
    TEST_WORKDIR
    form_dune_outputs_multiple_files
    REQUIRED_FIXTURES
    job:form_dune_outputs_multiple_files
  )

  # Legacy 'products: []' writes nothing and is not an error.
  cet_test(
    job:form_dune_outputs_empty_products
    HANDBUILT
    TEST_EXEC
    phlex::phlex
    TEST_ARGS
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/form_dune_outputs_empty_products.jsonnet
    TEST_WORKDIR
    form_dune_outputs_empty_products
    TEST_PROPERTIES
    ENVIRONMENT
    "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
  )

  # Invalid output configurations are rejected when form_module is registered.
  foreach(case conflict duplicate empty no_file)
    if(case STREQUAL "conflict")
      set(expected_error "cannot be combined with 'outputs'")
    elseif(case STREQUAL "duplicate")
      set(expected_error "configured more than once for output file")
    elseif(case STREQUAL "empty")
      set(expected_error "'outputs' must contain at least one output")
    else()
      set(expected_error "must specify 'output_file'")
    endif()
    cet_test(
      job:form_dune_outputs_${case}
      HANDBUILT
      TEST_EXEC
      phlex::phlex
      TEST_ARGS
      -c
      ${CMAKE_CURRENT_SOURCE_DIR}/form_dune_outputs_${case}.jsonnet
      TEST_PROPERTIES
      PASS_REGULAR_EXPRESSION
      "${expected_error}"
      ENVIRONMENT
      "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
    )
  endforeach()
endif()

if(FORM_USE_ROOT_STORAGE AND FORM_USE_RNTUPLE_STORAGE)
  # One file holding TTree and RNTuple outputs of the same (creator, stage).
  cet_test(
    job:form_dune_outputs_mixed_technologies
    HANDBUILT
    TEST_EXEC
    phlex::phlex
    TEST_ARGS
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/form_dune_outputs_mixed_technologies.jsonnet
    TEST_WORKDIR
    form_dune_outputs_mixed_technologies
    TEST_PROPERTIES
    ENVIRONMENT
    "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
  )
  cet_test(
    form_dune_outputs_mixed_technologies_check
    HANDBUILT
    TEST_EXEC
    form_multiple_outputs_check
    TEST_ARGS
    test
    dune_mixed.root
    ROOT_TTREE
    hitCandidates,wireCandidates
    dune_mixed.root
    ROOT_RNTUPLE
    wireCandidates,spillCandidates
    DIRTY_WORKDIR
    TEST_WORKDIR
    form_dune_outputs_mixed_technologies
    REQUIRED_FIXTURES
    job:form_dune_outputs_mixed_technologies
  )
endif()

set(form_basics_test_libraries form)
if(FORM_USE_ROOT_STORAGE)
  list(APPEND form_basics_test_libraries root_storage)
endif()
cet_test(form_basics USE_CATCH2_MAIN SOURCE form_basics_test.cpp LIBRARIES
         ${form_basics_test_libraries}
)
target_include_directories(form_basics PRIVATE ${PROJECT_SOURCE_DIR}/form)
if(FORM_USE_ROOT_STORAGE)
  target_compile_definitions(form_basics PRIVATE USE_ROOT_STORAGE)
  if(FORM_USE_RNTUPLE_STORAGE)
    target_compile_definitions(form_basics PRIVATE USE_RNTUPLE_STORAGE)
  endif()
endif()

add_library(generate_vector MODULE generate_vector.cpp)
target_link_libraries(generate_vector PRIVATE phlex::module)

add_library(form_source_extra_types MODULE form_source_extra_types.cpp)
target_link_libraries(form_source_extra_types PRIVATE phlex::module form)
target_include_directories(form_source_extra_types PRIVATE ${PROJECT_SOURCE_DIR})

add_library(passthrough_vector MODULE passthrough_vector.cpp)
target_link_libraries(passthrough_vector PRIVATE phlex::module)

cet_test(
  job:form_source_coverage_write_ttree
  HANDBUILT
  TEST_EXEC
  phlex::phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/form_source_write_ttree.jsonnet
  --stage test
  TEST_WORKDIR
  form_source_coverage_ttree
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
)

cet_test(
  job:form_source_coverage_read_ttree
  HANDBUILT
  TEST_EXEC
  phlex::phlex
  TEST_ARGS
  -c
  ${CMAKE_CURRENT_SOURCE_DIR}/form_source_read_ttree.jsonnet
  --stage test
  DIRTY_WORKDIR
  TEST_WORKDIR
  form_source_coverage_ttree
  REQUIRED_FIXTURES
  job:form_source_coverage_write_ttree
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}/${phlex_LIBRARY_DIR}:${CMAKE_BINARY_DIR}/form"
)

if(FORM_USE_ROOT_STORAGE AND FORM_USE_RNTUPLE_STORAGE)
  cet_test(
    "form_rntuple_error_handling_extra"
    USE_CATCH2_MAIN
    SOURCE
    form_rntuple_error_handling_extra.cpp
    LIBRARIES
    root_storage
    storage
    persistence
    form
    form_test_utils
    form_test_data_products
    TEST_PROPERTIES
    RESOURCE_LOCK
    "FORMTestFile.root"
  )
endif()
