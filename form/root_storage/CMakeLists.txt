# Copyright (C) 2025 ...

# Specify the ROOT dependencies
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)

set(FORM_ROOT_STORAGE_SOURCES)
list(
  APPEND FORM_ROOT_STORAGE_SOURCES
  root_descriptor.cpp
  root_tfile.cpp
  root_thread_safety.cpp
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
    handle_rexception.cpp
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

target_include_directories(root_storage PRIVATE ${ROOT_INCLUDE_DIRS})
reflex_generate_dictionary(root_storage descriptor_dict.h SELECTION classes_def.xml)

# Suppress a GCC false positive triggered by LTO when linking ROOT-generated dictionaries, the
# same workaround the test dictionaries use
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_options(root_storage PUBLIC "-Wno-alloc-size-larger-than")
endif()
