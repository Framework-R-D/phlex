include(CheckCXXSourceCompiles)

# Check if flow::resource_provider<int> is available in TBB
set(CMAKE_REQUIRED_LIBRARIES_SAVE ${CMAKE_REQUIRED_LIBRARIES})
set(CMAKE_REQUIRED_INCLUDES_SAVE ${CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_FLAGS_SAVE "${CMAKE_REQUIRED_FLAGS}")

get_target_property(_TBB_INCLUDES TBB::tbb INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_TBB_LOCATION TBB::tbb LOCATION)
if(_TBB_INCLUDES)
  set(CMAKE_REQUIRED_INCLUDES ${_TBB_INCLUDES})
endif()
if(_TBB_LOCATION)
  set(CMAKE_REQUIRED_LIBRARIES ${_TBB_LOCATION})
endif()
set(CMAKE_REQUIRED_FLAGS "-std=c++${CMAKE_CXX_STANDARD}")

check_cxx_source_compiles(
  "
  #define TBB_PREVIEW_FLOW_GRAPH_RESOURCE_LIMITED_NODE 1
  #include <oneapi/tbb/flow_graph.h>

  using type = oneapi::tbb::flow::resource_provider<int>;

  int main() {}
"
  HAVE_TBB_RESOURCE_PROVIDER
)

set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_SAVE})
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES_SAVE})
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_SAVE}")

if(HAVE_TBB_RESOURCE_PROVIDER)
  cet_test(
    resource_limiting
    USE_CATCH2_MAIN
    SOURCE
    resource_limiting_test.cpp
    LIBRARIES
    phlex::core
    TBB::tbb
    spdlog::spdlog
  )
  target_compile_definitions(
    resource_limiting
    PRIVATE TBB_PREVIEW_FLOW_GRAPH_RESOURCE_LIMITED_NODE=1
  )
else()
  message(
    STATUS
    "Skipping resource_limiting test: flow::resource_provider<int> not available in TBB"
  )
endif()
