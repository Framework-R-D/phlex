cet_make_library(
  LIBRARY_NAME
  phlex_core
  SHARED
  SOURCE
  concurrency.cpp
  consumer.cpp
  declared_fold.cpp
  declared_observer.cpp
  declared_output.cpp
  declared_predicate.cpp
  declared_provider.cpp
  declared_transform.cpp
  declared_unfold.cpp
  detail/make_algorithm_name.cpp
  detail/maybe_predicates.cpp
  detail/filter_impl.cpp
  detail/repeater_node.cpp
  edge_creation_policy.cpp
  edge_maker.cpp
  filter.cpp
  framework_graph.cpp
  glue.cpp
  input_arguments.cpp
  message.cpp
  node_catalog.cpp
  index_router.cpp
  products_consumer.cpp
  registrar.cpp
  registration_api.cpp
  product_query.cpp
  store_counters.cpp
  LIBRARIES
  PUBLIC
  TBB::tbb
  phlex::metaprogramming
  phlex::model
  phlex::utilities
  PRIVATE
  Boost::json
  spdlog::spdlog
)
install(
  FILES
    concepts.hpp
    consumer.hpp
    declared_fold.hpp
    declared_observer.hpp
    declared_output.hpp
    declared_predicate.hpp
    declared_provider.hpp
    declared_transform.hpp
    declared_unfold.hpp
    edge_creation_policy.hpp
    edge_maker.hpp
    filter.hpp
    framework_graph.hpp
    fwd.hpp
    glue.hpp
    graph_proxy.hpp
    input_arguments.hpp
    message.hpp
    index_router.hpp
    node_catalog.hpp
    product_query.hpp
    products_consumer.hpp
    registrar.hpp
    registration_api.hpp
    store_counters.hpp
    upstream_predicates.hpp
  DESTINATION include/phlex/core
)
install(FILES fold/send.hpp DESTINATION include/phlex/core/fold)
install(
  FILES
    detail/filter_impl.hpp
    detail/make_algorithm_name.hpp
    detail/maybe_predicates.hpp
    detail/repeater_node.hpp
  DESTINATION include/phlex/core/detail
)
target_include_directories(phlex_core PRIVATE ${PROJECT_SOURCE_DIR})

# AppleClang 15.0 still treats std::views::join as experimental
if(
  CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.5"
)
  target_compile_options(phlex_core PRIVATE "-fexperimental-library")
endif()

# Interface library
cet_make_library(
  LIBRARY_NAME
  phlex_core_int
  EXPORT_NAME
  core
  INTERFACE
  NO_SOURCE
  LIBRARIES
  phlex_core
  phlex::metaprogramming
  phlex::model
  phlex::utilities
  Boost::json
  TBB::tbb
)
