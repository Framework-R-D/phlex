# Copyright (C) 2025 ...

if(FORM_USE_ROOT_STORAGE)
  add_subdirectory(data_products)

  cet_test(
    WriteVector
    SOURCE
    writer.cpp
    toy_tracker.cpp
    LIBRARIES
    mock_phlex
    form
    form_test_data_products
    TEST_ARGS
    "${CMAKE_CURRENT_BINARY_DIR}/toy.root"
    )
  target_include_directories(WriteVector PRIVATE ${PROJECT_SOURCE_DIR}/form)

  cet_test(
    ReadVector
    SOURCE
    reader.cpp
    LIBRARIES
    mock_phlex
    form
    form_test_data_products
    TEST_ARGS
    "${CMAKE_CURRENT_BINARY_DIR}/toy.root"
    TEST_PROPERTIES
    DEPENDS
    WriteVector
    )
  target_include_directories(ReadVector PRIVATE ${PROJECT_SOURCE_DIR}/form)
endif()
