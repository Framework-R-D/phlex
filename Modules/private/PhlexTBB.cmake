# Provides phlex_check_tbb_resource_limiting(), which verifies that the
# TBB preview resource-limiting API required by Phlex is available.

include_guard()

include(CheckCXXSourceCompiles)

function(phlex_check_tbb_resource_limiting)
  set(_phlex_required_libraries_save ${CMAKE_REQUIRED_LIBRARIES})
  set(_phlex_required_includes_save ${CMAKE_REQUIRED_INCLUDES})
  set(_phlex_required_flags_save "${CMAKE_REQUIRED_FLAGS}")

  get_target_property(_phlex_tbb_includes TBB::tbb INTERFACE_INCLUDE_DIRECTORIES)
  if(_phlex_tbb_includes)
    set(CMAKE_REQUIRED_INCLUDES ${_phlex_tbb_includes})
  endif()
  set(CMAKE_REQUIRED_LIBRARIES TBB::tbb)
  set(CMAKE_REQUIRED_FLAGS "-std=c++${CMAKE_CXX_STANDARD}")

  check_cxx_source_compiles(
    "
    #define TBB_PREVIEW_FLOW_GRAPH_RESOURCE_LIMITING 1
    #include <oneapi/tbb/flow_graph.h>

    using type = oneapi::tbb::flow::resource_limiter<int>;

    int main() {}
    "
    HAVE_TBB_RESOURCE_LIMITING
  )

  set(CMAKE_REQUIRED_LIBRARIES ${_phlex_required_libraries_save})
  set(CMAKE_REQUIRED_INCLUDES ${_phlex_required_includes_save})
  set(CMAKE_REQUIRED_FLAGS "${_phlex_required_flags_save}")

  if(NOT HAVE_TBB_RESOURCE_LIMITING)
    message(FATAL_ERROR "Phlex requires TBB with flow::resource_limiter support")
  endif()
endfunction()
