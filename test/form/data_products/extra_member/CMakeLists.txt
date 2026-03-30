set(FORM_EXTRA_DATA_PROD_LIB_NAME "form_test_data_products_schema_evolution")
add_library(em_track_start_obj OBJECT track_start.cpp)
set_target_properties(em_track_start_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(
  em_track_start_obj
  PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include
)
add_library(${FORM_EXTRA_DATA_PROD_LIB_NAME} SHARED $<TARGET_OBJECTS:em_track_start_obj>)

if(FORM_USE_ROOT_STORAGE)
  find_package(ROOT REQUIRED COMPONENTS RIO)
  include_directories(${ROOT_INCLUDE_DIRS})
  reflex_generate_dictionary(
      ${FORM_EXTRA_DATA_PROD_LIB_NAME} dict.h SELECTION classes_def.xml
  )
  target_link_libraries(${FORM_EXTRA_DATA_PROD_LIB_NAME} PRIVATE ROOT::RIO)
  set_target_properties(${FORM_EXTRA_DATA_PROD_LIB_NAME} PROPERTIES CXX_CLANG_TIDY "")
endif()
