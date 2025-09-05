# Copyright (C) 2025 ...

# Component(s) in the package:
add_library(storage
	storage.cpp
	storage_file.cpp
	storage_container.cpp
	storage_association.cpp
	storage_associative_container.cpp
)

if (FORM_USE_ROOT_STORAGE)
	target_link_libraries(storage core root_storage)
endif()
