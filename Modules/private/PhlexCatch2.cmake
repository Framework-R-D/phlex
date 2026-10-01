include_guard()

include(FetchContent)

function(phlex_external_catch2_has_thread_safe_assertions result)
  find_package(Catch2 3.9 QUIET)
  if(NOT TARGET Catch2::Catch2)
    set(${result} FALSE PARENT_SCOPE)
    return()
  endif()

  get_target_property(catch2_include_dirs Catch2::Catch2 INTERFACE_INCLUDE_DIRECTORIES)
  foreach(catch2_include_dir IN LISTS catch2_include_dirs)
    set(catch2_user_config "${catch2_include_dir}/catch2/catch_user_config.hpp")
    if(EXISTS "${catch2_user_config}")
      file(
        STRINGS "${catch2_user_config}"
        catch2_thread_safe_assertions
        REGEX "^#define CATCH_CONFIG_THREAD_SAFE_ASSERTIONS$"
      )
      if(catch2_thread_safe_assertions)
        set(${result} TRUE PARENT_SCOPE)
        return()
      endif()
    endif()
  endforeach()

  set(${result} FALSE PARENT_SCOPE)
endfunction()

set(CATCH_CONFIG_NO_COUNTER ON)
set(CATCH_CONFIG_THREAD_SAFE_ASSERTIONS ON)

phlex_external_catch2_has_thread_safe_assertions(phlex_use_external_catch2)
if(NOT phlex_use_external_catch2)
  set(phlex_restore_find_package_mode FALSE)
  if(DEFINED FETCHCONTENT_TRY_FIND_PACKAGE_MODE)
    set(phlex_restore_find_package_mode TRUE)
    set(phlex_find_package_mode "${FETCHCONTENT_TRY_FIND_PACKAGE_MODE}")
  endif()
  set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
endif()

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.13.0
  GIT_SHALLOW ON
  EXCLUDE_FROM_ALL # Do not install
  FIND_PACKAGE_ARGS 3.9
)

if(NOT phlex_use_external_catch2)
  if(phlex_restore_find_package_mode)
    set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE "${phlex_find_package_mode}")
  else()
    unset(FETCHCONTENT_TRY_FIND_PACKAGE_MODE)
  endif()
endif()

FetchContent_MakeAvailable(Catch2)
