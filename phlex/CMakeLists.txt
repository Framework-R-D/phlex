cet_register_export_set(SET_NAME phlex NAMESPACE phlex SET_DEFAULT)

add_subdirectory(metaprogramming)
add_subdirectory(utilities)
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
install(FILES detail/plugin_macros.hpp DESTINATION include/phlex/detail)

cet_make_library(
  LIBRARY_NAME
  phlex_configuration_internal
  SHARED
  SOURCE
  configuration.cpp
  LIBRARIES
  PRIVATE
  Boost::json
  phlex::core
)

cet_make_library(
  LIBRARY_NAME
  phlex_configuration
  EXPORT_NAME
  configuration
  INTERFACE
  NO_SOURCE
  LIBRARIES
  Boost::json
  phlex::core
  phlex_configuration_internal
)
install(FILES configuration.hpp DESTINATION include/phlex)

add_subdirectory(app)
