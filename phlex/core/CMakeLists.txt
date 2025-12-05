cet_make_library(
  LIBRARY_NAME
  phlex_core
  SHARED
  SOURCE
  concurrency.cpp
  consumer.cpp
  declared_observer.cpp
  declared_output.cpp
  declared_predicate.cpp
  declared_fold.cpp
  declared_unfold.cpp
  declared_transform.cpp
  detail/make_algorithm_name.cpp
  detail/maybe_predicates.cpp
  detail/filter_impl.cpp
  edge_creation_policy.cpp
  end_of_message.cpp
  filter.cpp
  framework_graph.cpp
  glue.cpp
  input_arguments.cpp
  message.cpp
  message_sender.cpp
  node_catalog.cpp
  multiplexer.cpp
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
  phlex::graph
  Boost::json
  spdlog::spdlog
  )
install(
  FILES cached_product_stores.hpp
        concepts.hpp
        consumer.hpp
        declared_fold.hpp
        declared_observer.hpp
        declared_output.hpp
        declared_predicate.hpp
        declared_transform.hpp
        declared_unfold.hpp
        edge_creation_policy.hpp
        edge_maker.hpp
        end_of_message.hpp
        filter.hpp
        framework_graph.hpp
        fwd.hpp
        glue.hpp
        graph_proxy.hpp
        input_arguments.hpp
        message.hpp
        message_sender.hpp
        multiplexer.hpp
        node_catalog.hpp
        product_query.hpp
        products_consumer.hpp
        registrar.hpp
        registration_api.hpp
        store_counters.hpp
        upstream_predicates.hpp
  DESTINATION include/phlex/core
  )
target_include_directories(phlex_core PRIVATE ${PROJECT_SOURCE_DIR})

# AppleClang 15.0 still treats std::views::join as experimental
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION
                                                   VERSION_GREATER_EQUAL "14.5"
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
  phlex::graph
  phlex::model
  phlex::utilities
  Boost::json
  TBB::tbb
  )
