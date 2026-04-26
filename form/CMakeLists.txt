# ##############################################################################
# FORM integration
#
# Marshal dependencies with find_package() (and reflect in Spack recipe) before
# building code.
#
# Place tests in <top>/test/form.
# ##############################################################################
# Copyright (C) 2025 ...

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${PROJECT_SOURCE_DIR}/form)

# ROOT Storage toggle
option(FORM_USE_ROOT_STORAGE "Enable ROOT Storage" ON)

# Add sub directories
add_subdirectory(form)
add_subdirectory(core)
add_subdirectory(util)
add_subdirectory(persistence)
add_subdirectory(storage)
if(FORM_USE_ROOT_STORAGE)
  add_subdirectory(root_storage)
endif()

add_library(form_module MODULE form_module.cpp)
target_link_libraries(form_module PRIVATE phlex::module form)
target_include_directories(form_module PRIVATE ${PROJECT_SOURCE_DIR})

add_library(form_source MODULE form_source.cpp)
target_link_libraries(form_source PRIVATE phlex::module form)
target_include_directories(form_source PRIVATE ${PROJECT_SOURCE_DIR})

install(TARGETS form_module form_source LIBRARY DESTINATION lib)
