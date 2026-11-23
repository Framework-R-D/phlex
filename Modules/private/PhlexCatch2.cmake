include_guard()

include(FetchContent)

set(CATCH_CONFIG_NO_COUNTER ON)
set(CATCH_CONFIG_THREAD_SAFE_ASSERTIONS ON)

# TODO: Revisit this temporary probe-and-replace machinery: require external
# Catch2 packages to provide the needed features and fail clearly if unsuitable.
# Fetch only when absent, if still desired, and simplify test_catch2_cmake.py accordingly.

# Imported targets are directory-scoped, not function-scoped. Probe in a child
# directory so a rejected package cannot collide with the fallback's targets.
add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/catch2-probe"
  "${CMAKE_CURRENT_BINARY_DIR}/catch2-probe"
)
if(phlex_use_external_catch2)
  find_package(Catch2 3.9 REQUIRED)
else()
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.13.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL # Do not install
    OVERRIDE_FIND_PACKAGE
  )
  FetchContent_MakeAvailable(Catch2)
endif()

# The fallback defines this in its generated header. Only add a consumer
# definition for external packages whose header does not already define it.
if(phlex_use_external_catch2 AND NOT phlex_catch2_has_no_counter)
  get_target_property(phlex_catch2_target Catch2::Catch2 ALIASED_TARGET)
  if(NOT phlex_catch2_target)
    set(phlex_catch2_target Catch2::Catch2)
  endif()
  target_compile_definitions(${phlex_catch2_target} INTERFACE CATCH_CONFIG_NO_COUNTER)
endif()
