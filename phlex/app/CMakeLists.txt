configure_file(version.cpp.in version.cpp @ONLY)

cet_make_library(
  LIBRARY_NAME
  run_phlex
  SHARED
  SOURCE
  load_module.cpp
  run.cpp
  version.cpp
  LIBRARIES
  PUBLIC
  # FIXME: We probably also need to add (and/or replace with) the future library
  # that will be including phlex/driver.hpp
  phlex::core
  Boost::json
)

install(FILES load_module.hpp run.hpp version.hpp DESTINATION include/phlex/app)

# We'll use C++17's filesystem instead of Boost's
target_compile_definitions(run_phlex PRIVATE BOOST_DLL_USE_STD_FS)

add_executable(phlex phlex.cpp)
target_link_libraries(
  phlex
  PRIVATE Boost::json Boost::program_options run_phlex phlex::core jsonnet::lib
)

set_target_properties(phlex PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
install(TARGETS phlex RUNTIME DESTINATION bin)
