# Interface library
cet_make_library(
  LIBRARY_NAME
  phlex_metaprogramming_int
  EXPORT_NAME
  metaprogramming
  INTERFACE
  NO_SOURCE
  LIBRARIES
  Boost::boost
)

install(FILES delegate.hpp type_deduction.hpp DESTINATION include/phlex/metaprogramming)

install(
  FILES
    detail/basic_concepts.hpp
    detail/ctor_reflect_types.hpp
    detail/number_output_objects.hpp
    detail/number_parameters.hpp
    detail/parameter_types.hpp
    detail/return_type.hpp
  DESTINATION include/phlex/metaprogramming/detail
)

add_library(phlex::metaprogramming ALIAS phlex_metaprogramming_int)
