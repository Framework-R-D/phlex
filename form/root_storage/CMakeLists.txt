# Copyright (C) 2025 ...

# Specify the ROOT dependencies
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)

# Component(s) in the package:
add_library(root_storage root_tfile.cpp root_ttree_read_container.cpp root_ttree_write_container.cpp root_tbranch_read_container.cpp root_tbranch_write_container.cpp demangle_name.cpp)
target_compile_definitions(root_storage PUBLIC USE_ROOT_STORAGE)

# Link the ROOT libraries
target_link_libraries(root_storage PUBLIC ROOT::Core ROOT::RIO ROOT::Tree storage)
