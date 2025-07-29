########################################################################
# Tests of FORM integration
#
# Marshal dependencies with find_package() (and reflect in Spack recipe)
# before building code.
#
# Place tests in <top>/test/form
########################################################################
include_directories(${PROJECT_SOURCE_DIR}/form)

if(FORM_USE_ROOT_STORAGE)
	add_subdirectory(data_products)
endif()

add_executable(phlex_writer writer.cpp)
target_link_libraries(phlex_writer form form_test_data_products)

add_executable(phlex_reader reader.cpp)
target_link_libraries(phlex_reader form form_test_data_products)

add_test(NAME WriteVector COMMAND phlex_writer)
add_test(NAME ReadVector COMMAND phlex_reader)
