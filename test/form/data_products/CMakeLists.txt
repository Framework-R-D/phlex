set(FORM_DATA_PROD_LIB_NAME "form_test_data_products")
add_library(${FORM_DATA_PROD_LIB_NAME}_obj OBJECT track_start.cpp unserializable.cpp)
set_target_properties(${FORM_DATA_PROD_LIB_NAME}_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(
  ${FORM_DATA_PROD_LIB_NAME}_obj
  PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include
)
add_library(${FORM_DATA_PROD_LIB_NAME} SHARED $<TARGET_OBJECTS:${FORM_DATA_PROD_LIB_NAME}_obj>)

set_target_properties(
  ${FORM_DATA_PROD_LIB_NAME}
  PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(FORM_USE_ROOT_STORAGE)
  find_package(ROOT REQUIRED COMPONENTS RIO)
  include_directories(${ROOT_INCLUDE_DIRS})
  reflex_generate_dictionary(
      ${FORM_DATA_PROD_LIB_NAME} dict.h SELECTION classes_def.xml
  )
  target_link_libraries(${FORM_DATA_PROD_LIB_NAME} PRIVATE ROOT::RIO)

  add_subdirectory(extra_member)
  add_subdirectory(dune_example)
endif()
