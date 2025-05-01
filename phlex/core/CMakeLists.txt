add_library(
  phlex_core SHARED
  concurrency.cpp
  consumer.cpp
  declared_observer.cpp
  declared_output.cpp
  declared_predicate.cpp
  declared_fold.cpp
  declared_unfold.cpp
  declared_transform.cpp
  detail/filter_impl.cpp
  dot/attributes.cpp
  dot/data_graph.cpp
  dot/function_graph.cpp
  edge_creation_policy.cpp
  edge_maker.cpp
  end_of_message.cpp
  filter.cpp
  framework_graph.cpp
  message.cpp
  message_sender.cpp
  multiplexer.cpp
  products_consumer.cpp
  specified_label.cpp
  store_counters.cpp
  )
target_include_directories(phlex_core PRIVATE ${PROJECT_SOURCE_DIR})

# AppleClang 15.0 still treats std::views::join as experimental
if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.5"
    )
  target_compile_options(phlex_core PRIVATE "-fexperimental-library")
endif ()

target_link_libraries(
  phlex_core PRIVATE TBB::tbb phlex::graph phlex::metaprogramming phlex::model
                     phlex::utilities PUBLIC spdlog::spdlog
  )

# Interface library
add_library(phlex_core_int INTERFACE)
target_include_directories(
  phlex_core_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                           "$<INSTALL_INTERFACE:include>"
  )
target_link_libraries(
  phlex_core_int
  INTERFACE phlex_core
            phlex::metaprogramming
            phlex::graph
            phlex::model
            phlex::utilities
            Boost::json
            TBB::tbb
  )

add_library(phlex::core ALIAS phlex_core_int)

install(TARGETS phlex_core)
