# Copyright (C) 2025 ...

# Specify the ROOT dependencies
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)

set(FORM_ROOT_STORAGE_SOURCES)
list(
  APPEND FORM_ROOT_STORAGE_SOURCES
  root_tfile.cpp
  root_ttree_write_container.cpp
  root_tbranch_read_container.cpp
  root_tbranch_write_container.cpp
  demangle_name.cpp
)

if(FORM_USE_RNTUPLE_STORAGE)
  list(
    APPEND FORM_ROOT_STORAGE_SOURCES
    root_rfield_read_container.cpp
    root_rfield_write_container.cpp
    root_rntuple_write_container.cpp
  )
endif()

# Component(s) in the package:
add_library(root_storage ${FORM_ROOT_STORAGE_SOURCES})
target_compile_definitions(root_storage PUBLIC USE_ROOT_STORAGE)
if(FORM_USE_RNTUPLE_STORAGE)
  target_compile_definitions(root_storage PUBLIC USE_RNTUPLE_STORAGE)
endif()

# Link the ROOT libraries
list(APPEND FORM_ROOT_STORAGE_DEPS ROOT::Core ROOT::RIO ROOT::Tree storage)

if(FORM_USE_RNTUPLE_STORAGE)
  list(APPEND FORM_ROOT_STORAGE_DEPS ROOT::ROOTNTuple)
endif()

target_link_libraries(root_storage PUBLIC ${FORM_ROOT_STORAGE_DEPS} PRIVATE Microsoft.GSL::GSL)
