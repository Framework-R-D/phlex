#Copyright(C) 2025 ...

#External dependencies:
#find_package(PHLEX)

if (FORM_USE_ROOT_STORAGE)
  add_subdirectory(data_products)

  add_executable(phlex_writer writer.cpp toy_tracker.cpp)
  target_include_directories(phlex_writer PRIVATE ${PROJECT_SOURCE_DIR}/form)
  target_link_libraries(phlex_writer mock_phlex form form_test_data_products)

  add_executable(phlex_reader reader.cpp)
  target_include_directories(phlex_reader PRIVATE ${PROJECT_SOURCE_DIR}/form)
  target_link_libraries(phlex_reader mock_phlex form form_test_data_products)

  add_test(NAME WriteVector COMMAND phlex_writer)
  add_test(NAME ReadVector COMMAND phlex_reader)
  set_tests_properties(ReadVector PROPERTIES DEPENDS WriteVector)
endif()
