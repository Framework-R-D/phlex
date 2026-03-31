set(FORM_DATA_PROD_LIB_NAME "form_test_data_products")
add_library(track_start_obj OBJECT track_start.cpp)
set_target_properties(track_start_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(track_start_obj PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include)
add_library(${FORM_DATA_PROD_LIB_NAME} SHARED $<TARGET_OBJECTS:track_start_obj>)

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
  set_target_properties(${FORM_DATA_PROD_LIB_NAME} PROPERTIES CXX_CLANG_TIDY "")

  add_subdirectory(extra_member)
endif()
