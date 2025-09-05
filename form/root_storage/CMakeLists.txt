# Copyright (C) 2025 ...

# Specify the ROOT dependencies
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)

# Component(s) in the package:
add_library(root_storage
	root_tfile.cpp
	root_ttree_container.cpp
	root_tbranch_container.cpp
)

# Link the ROOT libraries
target_link_libraries(root_storage PUBLIC ROOT::Core ROOT::RIO ROOT::Tree storage)
