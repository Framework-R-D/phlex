set(FORM_DATA_PROD_LIB_NAME "form_test_data_products")
add_library(
  ${FORM_DATA_PROD_LIB_NAME} SHARED track_start.cpp
  ) # REFLEX_GENERATE_DICTIONARY doesn't work trivially without making a shared
    # library
target_link_libraries(${FORM_DATA_PROD_LIB_NAME})

if(FORM_USE_ROOT_STORAGE)
  find_package(ROOT REQUIRED COMPONENTS Core RIO)
  include_directories(${ROOT_INCLUDE_DIRS})

  reflex_generate_dictionary(
    ${FORM_DATA_PROD_LIB_NAME} dict.h SELECTION classes_def.xml
    )
  target_compile_options(${FORM_DATA_PROD_LIB_NAME} PRIVATE "-w")
endif()
