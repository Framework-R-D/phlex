# Copyright (C) 2025 ...

# External dependencies:
#find_package( PHLEX )

if(USE_ROOT_STORAGE)
  add_subdirectory(data_products)
endif()

add_executable(phlex_writer writer.cpp config.cpp toy_tracker.cpp)
#add_executable(phlex_writer writer.cpp)
target_link_libraries(phlex_writer form form_test_data_products)

#add_executable(phlex_reader reader.cpp)
add_executable(phlex_reader reader.cpp config.cpp) 
target_link_libraries(phlex_reader form form_test_data_products)

add_test(NAME WriteVector COMMAND phlex_writer)
add_test(NAME ReadVector COMMAND phlex_reader)
