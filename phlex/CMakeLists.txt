add_subdirectory(app)
add_subdirectory(core)
add_subdirectory(graph)
add_subdirectory(metaprogramming)
add_subdirectory(model)
add_subdirectory(utilities)

if (PHLEX_USE_FORM)
  add_subdirectory(form)
endif()

# Interface library
add_library(phlex_int INTERFACE)
target_include_directories(
  phlex_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                      "$<INSTALL_INTERFACE:include>"
  )
target_link_libraries(phlex_int INTERFACE Boost::boost Boost::json phlex::core)

add_library(phlex::module ALIAS phlex_int)
