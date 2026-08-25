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
target_compile_definitions(resource_limiting PRIVATE TBB_PREVIEW_FLOW_GRAPH_RESOURCE_LIMITING=1)
