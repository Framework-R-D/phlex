# Copyright (C) 2025 ...

# Specify the ROOT dependencies
set(root_storage_ROOT_components)
set(root_storage_ROOT_libs)
list(APPEND root_storage_ROOT_components Core RIO Tree)
list(APPEND root_storage_ROOT_libs ROOT::Core ROOT::RIO ROOT::Tree storage)

if(FORM_USE_RNTUPLE_STORAGE)
  list(APPEND root_storage_ROOT_components ROOTNTuple)
  list(APPEND root_storage_ROOT_libs ROOT::ROOTNTuple)
endif(FORM_USE_RNTUPLE_STORAGE)

find_package(ROOT REQUIRED COMPONENTS ${root_storage_ROOT_components})

# Component(s) in the package:
set(root_storage_SOURCES)
list(APPEND root_storage_SOURCES
     root_tfile.cpp
     root_ttree_container.cpp
     root_tbranch_container.cpp
)

if(FORM_USE_RNTUPLE_STORAGE)
list(APPEND root_storage_SOURCES
     root_rntuple_container.cpp
     root_rfield_container.cpp
)
endif(FORM_USE_RNTUPLE_STORAGE)

add_library(root_storage ${root_storage_SOURCES})

# Link the ROOT libraries
target_link_libraries(root_storage PUBLIC ${root_storage_ROOT_libs})
