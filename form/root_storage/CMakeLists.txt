# Copyright (C) 2025 ...

# Specify the ROOT dependencies
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)

# Include ROOT headers
include_directories(${ROOT_INCLUDE_DIRS})

# Link ROOT libraries
link_directories(${ROOT_LIBRARY_DIRS})
add_definitions(${ROOT_CXX_FLAGS})

# Component(s) in the package:
add_library(root_storage
	root_tfile.cpp
	root_ttree_container.cpp
)
# Link the ROOT libraries
target_link_libraries(root_storage ${ROOT_LIBRARIES})
