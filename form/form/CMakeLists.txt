# Copyright (C) 2025 ...

# External dependencies:
#find_package( PHLEX )

# Component(s) in the package:
add_library(form
	form.cpp
        parse_config.cpp
      )

# Set up include directories so form.hpp can find mock_phlex headers
target_include_directories(form PRIVATE ${PROJECT_SOURCE_DIR}/form/)

target_link_libraries(form persistence)
target_link_libraries(form mock_phlex)
