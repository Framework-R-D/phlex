# Copyright (C) 2025 ...

# Component(s) in the package:
add_library(
  persistence
  navigation_catalog.cpp
  persistence_reader.cpp
  persistence_writer.cpp
  persistence_utils.cpp
)
target_link_libraries(persistence core storage form_util)
