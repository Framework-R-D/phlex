# Copyright (C) 2025 ...

if(FORM_USE_ROOT_STORAGE)
  add_subdirectory(data_products)
  cet_test(
      WriteVector
      SOURCE
      writer.cpp
      toy_tracker.cpp
      LIBRARIES
      form
      form_test_data_products
      TEST_ARGS
      "${CMAKE_CURRENT_BINARY_DIR}/toy.root"
      "${CMAKE_CURRENT_BINARY_DIR}/toy_checksums.txt"
  )
  target_include_directories(WriteVector PRIVATE ${PROJECT_SOURCE_DIR}/form)

  cet_test(
      ReadVector
      SOURCE
      reader.cpp
      LIBRARIES
      form
      form_test_data_products
      TEST_ARGS
      "${CMAKE_CURRENT_BINARY_DIR}/toy.root"
      "${CMAKE_CURRENT_BINARY_DIR}/toy_checksums.txt"
      TEST_PROPERTIES
      DEPENDS
      WriteVector
  )
  target_include_directories(ReadVector PRIVATE ${PROJECT_SOURCE_DIR}/form)

  cet_test(
      form_root_schema_write_test
      SOURCE
      form_root_schema_write_test.cpp
      toy_tracker.cpp
      LIBRARIES
      form
      form_test_data_products
      TEST_WORKDIR
      form_root_schema_test
  )
  target_include_directories(form_root_schema_write_test PRIVATE ${PROJECT_SOURCE_DIR}/form)

  cet_test(
     form_root_schema_read_test
     SOURCE
     form_root_schema_read_test.cpp
     LIBRARIES
     form
     form_test_data_products_schema_evolution
     DIRTY_WORKDIR
     TEST_WORKDIR
     form_root_schema_test
     REQUIRED_FIXTURES
     form_root_schema_write_test
  )
  target_include_directories(form_root_schema_read_test PRIVATE ${PROJECT_SOURCE_DIR}/form)

  cet_test(
    check_root_schema_evolution
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
    form_root_schema_write_test
    form_root_schema_read_test
  )
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

cet_test(form_storage_test USE_CATCH2_MAIN SOURCE form_storage_test.cpp LIBRARIES
         root_storage
         storage
)
target_include_directories(form_storage_test PRIVATE ${PROJECT_SOURCE_DIR}/form)
