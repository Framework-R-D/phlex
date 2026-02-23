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
