set(FORM_DUNE_EXAMPLE_DATA_PROD_LIB_NAME "form_test_dune_example_data_products")
add_library(dune_example_data_products_obj OBJECT hit.cpp hit_candidate.cpp)
set_target_properties(dune_example_data_products_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(
  dune_example_data_products_obj
  PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include
)
add_library(
  ${FORM_DUNE_EXAMPLE_DATA_PROD_LIB_NAME} SHARED $<TARGET_OBJECTS:dune_example_data_products_obj>
)

set_target_properties(
  ${FORM_DUNE_EXAMPLE_DATA_PROD_LIB_NAME}
  PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(FORM_USE_ROOT_STORAGE)
  find_package(ROOT REQUIRED COMPONENTS RIO)
  include_directories(${ROOT_INCLUDE_DIRS})
  reflex_generate_dictionary(
      ${FORM_DUNE_EXAMPLE_DATA_PROD_LIB_NAME} dict.h SELECTION classes_def.xml
  )
  target_link_libraries(${FORM_DUNE_EXAMPLE_DATA_PROD_LIB_NAME} PRIVATE ROOT::RIO)
endif()
