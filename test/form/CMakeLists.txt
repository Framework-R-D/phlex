# Copyright (C) 2025 ...

if(FORM_USE_ROOT_STORAGE)
  add_subdirectory(data_products)

  find_package(ROOT REQUIRED COMPONENTS Core)
  add_library(form_test_utils INTERFACE)
  target_link_libraries(form_test_utils INTERFACE ROOT::Core)
  target_include_directories(form_test_utils INTERFACE ${PROJECT_SOURCE_DIR})

  list(APPEND FORM_TEST_TECHNOLOGIES "ROOT_TTREE")
  #if(FORM_USE_RNTUPLE_STORAGE) #Example of what this loop will be used for
  #  list(APPEND FORM_TEST_TECHNOLOGIES "ROOT_RNTUPLE")
  #endif()

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
        "${CMAKE_CURRENT_BINARY_DIR}/toy.root"
        "${CMAKE_CURRENT_BINARY_DIR}/toy_checksums.txt"
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
        "${CMAKE_CURRENT_BINARY_DIR}/toy.root"
        "${CMAKE_CURRENT_BINARY_DIR}/toy_checksums.txt"
        ${TECH}
        TEST_PROPERTIES
        DEPENDS
        "WriteVector_${TECH}"
    )
    target_include_directories("ReadVector_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)

    cet_test(
        "form_root_schema_write_test_${TECH}"
        SOURCE
        form_root_schema_write_test.cpp
        toy_tracker.cpp
        LIBRARIES
        form
        root_storage
        form_test_data_products
        form_test_utils
        TEST_WORKDIR
        form_root_schema_test
        TEST_ARGS
        ${TECH}
    )
    target_include_directories(
      "form_root_schema_write_test_${TECH}"
      PRIVATE ${PROJECT_SOURCE_DIR}/form
    )

    cet_test(
       "form_root_schema_read_test_${TECH}"
       SOURCE
       form_root_schema_read_test.cpp
       LIBRARIES
       form
       root_storage
       form_test_data_products_schema_evolution
       form_test_utils
       DIRTY_WORKDIR
       TEST_WORKDIR
       form_root_schema_test
       REQUIRED_FIXTURES
       "form_root_schema_write_test_${TECH}"
       TEST_ARGS
       ${TECH}
    )
    target_include_directories(
      "form_root_schema_read_test_${TECH}"
      PRIVATE ${PROJECT_SOURCE_DIR}/form
    )

    cet_test(
      "check_root_schema_evolution_${TECH}"
      HANDBUILT
      TEST_EXEC
      ${CMAKE_COMMAND}
      TEST_ARGS
      -E
      compare_files
      form_root_schema_write_log.txt
      form_root_schema_read_log.txt
      DIRTY_WORKDIR
      TEST_WORKDIR
      form_root_schema_test
      REQUIRED_FIXTURES
      "form_root_schema_write_test_${TECH}"
      "form_root_schema_read_test_${TECH}"
    )

    cet_test("form_storage_test_${TECH}" USE_CATCH2_MAIN SOURCE form_storage_test.cpp LIBRARIES
             root_storage
             storage
             form_test_utils
             TEST_ARGS
             --technology
             ${TECH}
    )
    target_include_directories("form_storage_test_${TECH}" PRIVATE ${PROJECT_SOURCE_DIR}/form)
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
  TEST_PROPERTIES
  ENVIRONMENT
  "PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR}:${CMAKE_BINARY_DIR}/form"
)

cet_test(form_basics_test USE_CATCH2_MAIN SOURCE form_basics_test.cpp LIBRARIES
         form
)
target_include_directories(form_basics_test PRIVATE ${PROJECT_SOURCE_DIR}/form)
