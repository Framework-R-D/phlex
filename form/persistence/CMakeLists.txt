# Copyright (C) 2025 ...

# Component(s) in the package:
add_library(persistence persistence.cpp)
target_link_libraries(persistence core storage)
