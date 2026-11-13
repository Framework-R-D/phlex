# Keep probe targets local even if the parent promotes imported targets globally.
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL FALSE)
set(phlex_use_external_catch2 FALSE PARENT_SCOPE)
set(phlex_catch2_has_no_counter FALSE PARENT_SCOPE)
find_package(Catch2 3.9 QUIET)
if(NOT Catch2_FOUND OR NOT TARGET Catch2::Catch2)
  return()
endif()

get_target_property(catch2_include_dirs Catch2::Catch2 INTERFACE_INCLUDE_DIRECTORIES)
foreach(catch2_include_dir IN LISTS catch2_include_dirs)
  set(catch2_user_config "${catch2_include_dir}/catch2/catch_user_config.hpp")
  if(EXISTS "${catch2_user_config}")
    file(
      STRINGS "${catch2_user_config}"
      catch2_thread_safe_assertions
      REGEX "^[ \t]*#[ \t]*define[ \t]+CATCH_CONFIG_THREAD_SAFE_ASSERTIONS([ \t]|$)"
    )
    if(catch2_thread_safe_assertions)
      file(
        STRINGS "${catch2_user_config}"
        catch2_no_counter
        REGEX "^[ \t]*#[ \t]*define[ \t]+CATCH_CONFIG_NO_COUNTER([ \t]|$)"
      )
      if(catch2_no_counter)
        set(phlex_catch2_has_no_counter TRUE PARENT_SCOPE)
      endif()
      set(phlex_use_external_catch2 TRUE PARENT_SCOPE)
      return()
    endif()
  endif()
endforeach()
