set(FORM_DATA_PROD_LIB_NAME "form_test_data_products")
add_library(${FORM_DATA_PROD_LIB_NAME} SHARED track_start.cpp) #ROOT_GENERATE_DICTIONARY doesn't work trivially without making a shared library
target_link_libraries(${FORM_DATA_PROD_LIB_NAME})

find_package(ROOT REQUIRED COMPONENTS Core RIO Tree) #Only used for ROOT_GENERATE_DICTIONARY for now
 include_directories(${ROOT_INCLUDE_DIRS})

ROOT_GENERATE_DICTIONARY(${FORM_DATA_PROD_LIB_NAME}_dict dict.h LINKDEF LinkDef.h MODULE ${FORM_DATA_PROD_LIB_NAME})
target_compile_options(${FORM_DATA_PROD_LIB_NAME}_dict PRIVATE "-w")
