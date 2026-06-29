# Copyright (C) 2025 ...

# External dependencies: find_package( PHLEX )

# Component(s) in the package:
add_library(form form_reader.cpp form_writer.cpp config.cpp form_source_type_registry.cpp)

target_link_libraries(form persistence phlex::model)

target_include_directories(form PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/form)
