cet_register_export_set(SET_NAME phlex NAMESPACE phlex SET_DEFAULT)

add_subdirectory(metaprogramming)
add_subdirectory(utilities)
add_subdirectory(graph)
add_subdirectory(model)
add_subdirectory(core)

cet_make_library(
  LIBRARY_NAME
  phlex_module
  EXPORT_NAME
  module
  INTERFACE
  NO_SOURCE
  LIBRARIES
  Boost::boost
  Boost::json
  phlex::core
  phlex::configuration
  )

install(FILES concurrency.hpp module.hpp source.hpp DESTINATION include/phlex)

cet_make_library(
  LIBRARY_NAME
  phlex_configuration
  EXPORT_NAME
  configuration
  INTERFACE
  SOURCE
  configuration.cpp
  LIBRARIES
  Boost::json
  phlex::core
  )
install(FILES configuration.hpp DESTINATION include/phlex)

add_subdirectory(app)
