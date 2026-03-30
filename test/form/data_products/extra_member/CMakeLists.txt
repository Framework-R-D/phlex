set(FORM_EXTRA_DATA_PROD_LIB_NAME "form_test_data_products_schema_evolution")
add_library(${FORM_EXTRA_DATA_PROD_LIB_NAME} SHARED track_start.cpp)

if(FORM_USE_ROOT_STORAGE)
  find_package(ROOT REQUIRED COMPONENTS RIO)
  include_directories(${ROOT_INCLUDE_DIRS})
  reflex_generate_dictionary(
      ${FORM_EXTRA_DATA_PROD_LIB_NAME} dict.h SELECTION classes_def.xml
  )
  target_link_libraries(${FORM_EXTRA_DATA_PROD_LIB_NAME} PRIVATE ROOT::RIO)
endif()
