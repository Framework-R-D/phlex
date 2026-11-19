# Copyright (C) 2025 ...

# Component(s) in the package:
add_library(
  storage
  descriptor.cpp
  factories.cpp
  storage_reader.cpp
  storage_writer.cpp
  storage_file.cpp
  storage_read_container.cpp
  storage_write_container.cpp
  storage_write_association.cpp
  storage_associative_write_container.cpp
)

list(APPEND STORAGE_LIBS form_util)
if(FORM_USE_ROOT_STORAGE)
  list(APPEND STORAGE_LIBS root_storage)
endif()
target_link_libraries(storage PUBLIC core PRIVATE ${STORAGE_LIBS})
