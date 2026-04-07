# Copyright (C) 2025 ...

# Component(s) in the package:
add_library(persistence persistence_reader.cpp persistence_writer.cpp persistence_utils.cpp)
target_link_libraries(persistence core storage)
