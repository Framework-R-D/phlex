# ##############################################################################
# FORM integration
#
# Marshal dependencies with find_package() (and reflect in Spack recipe) before
# building code.
#
# Place tests in <top>/test/form.
# ##############################################################################
# Copyright (C) 2025 ...

include_directories(${PROJECT_SOURCE_DIR}/form)

# ROOT Storage toggle
option(FORM_USE_ROOT_STORAGE "Enable ROOT Storage" ON)
if(FORM_USE_ROOT_STORAGE)
  add_definitions(-DUSE_ROOT_STORAGE)
endif()

# Add sub directories
add_subdirectory(form)
add_subdirectory(mock_phlex)
add_subdirectory(core)
add_subdirectory(util)
add_subdirectory(persistence)
add_subdirectory(storage)
if(FORM_USE_ROOT_STORAGE)
  add_subdirectory(root_storage)
endif()
