########################################################################
# Tests of FORM integration
#
# Marshal dependencies with find_package() (and reflect in Spack recipe)
# before building code.
#
# Place tests in <top>/test/form
########################################################################
include_directories(${PROJECT_SOURCE_DIR}/form)

add_executable(phlex_writer writer.cpp)
target_link_libraries(phlex_writer form)

add_executable(phlex_reader reader.cpp)
target_link_libraries(phlex_reader form)

add_test(NAME WriteVector COMMAND phlex_writer)
add_test(NAME ReadVector COMMAND phlex_reader)
