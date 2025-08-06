# Copyright (C) 2025 ...

# External dependencies:
#find_package( PHLEX )

# Component(s) in the package:
add_library(form
	form.cpp
	phlex_toy_core.cpp
	phlex_toy_config.cpp
)
target_link_libraries(form persistence)
