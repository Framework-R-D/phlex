add_library(index_router SHARED index_router.cpp)
target_link_libraries(index_router PRIVATE TBB::tbb phlex::model)

cet_test(
  repeater
  USE_CATCH2_MAIN
  SOURCE
  repeater.cpp
  LIBRARIES
  phlex::core
  index_router
  layer_generator
)
