# Copyright (C) 2025 ...

# Component(s) in the package:
add_library(
  storage
  storage_reader.cpp
  storage_writer.cpp
  storage_file.cpp
  storage_read_container.cpp
  storage_write_container.cpp
  storage_write_association.cpp
  storage_associative_write_container.cpp
)

if(FORM_USE_ROOT_STORAGE)
  target_link_libraries(
    storage
    PUBLIC core
    PRIVATE root_storage
  )
endif()
