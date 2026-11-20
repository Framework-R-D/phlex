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

target_link_libraries(
  root_storage
  PUBLIC ${FORM_ROOT_STORAGE_DEPS}
  PRIVATE Microsoft.GSL::GSL form_util
)

# ROOT dictionary for the descriptor's on-disk type.
add_library(root_storage_dict OBJECT)
target_include_directories(
  root_storage_dict
  PRIVATE ${PROJECT_SOURCE_DIR}/form ${ROOT_INCLUDE_DIRS}
)
reflex_generate_dictionary(root_storage_dict descriptor_dict.h SELECTION classes_def.xml)

# Link directly into form to prevent the dictionary object from being discarded as an unreferenced
# member of the root_storage static library.
target_link_libraries(form PRIVATE root_storage_dict)

# Ensure the PCM is generated before copying it next to libphlex_form.
add_dependencies(root_storage root_storage_dict)
add_custom_command(
  TARGET root_storage
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/root_storage_dict_rdict.pcm
    $<TARGET_FILE_DIR:form>
  VERBATIM
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/root_storage_dict_rdict.pcm DESTINATION lib)

# Suppress a GCC false positive triggered by LTO when linking ROOT-generated dictionaries, the
# same workaround the test dictionaries use
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_options(root_storage PUBLIC "-Wno-alloc-size-larger-than")
endif()
