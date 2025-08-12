# Copyright (C) 2025 ...

# External dependencies:
#find_package( PHLEX )

#if(FORM_USE_ROOT_STORAGE)
add_subdirectory(data_products)
#endif()

add_executable(phlex_writer writer.cpp config.cpp toy_tracker.cpp)
target_include_directories(phlex_writer PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(phlex_writer PRIVATE ${PROJECT_SOURCE_DIR}/form)
target_link_libraries(phlex_writer form form_test_data_products)

add_executable(phlex_reader reader.cpp config.cpp)
target_include_directories(phlex_reader PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(phlex_reader PRIVATE ${PROJECT_SOURCE_DIR}/form)
target_link_libraries(phlex_reader form form_test_data_products)

add_test(NAME WriteVector COMMAND phlex_writer)
add_test(NAME ReadVector COMMAND phlex_reader)
