# Copyright (C) 2025 ...

# External dependencies: find_package( PHLEX )

# Component(s) in the package:
add_library(form SHARED form_reader.cpp form_writer.cpp config.cpp form_source_type_registry.cpp)

target_link_libraries(form PRIVATE persistence phlex::model)

target_include_directories(
	form
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/form>
		$<INSTALL_INTERFACE:include/phlex>
)

add_library(phlex::form ALIAS form)
set_target_properties(form PROPERTIES EXPORT_NAME form)

install(TARGETS form EXPORT phlex LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES form_source_type_registry.hpp DESTINATION include/phlex/form/form)
