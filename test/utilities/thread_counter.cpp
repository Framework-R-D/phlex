#include "phlex/utilities/thread_counter.hpp"
#include "phlex/utilities/sleep_for.hpp"

#include "oneapi/tbb/flow_graph.h"
#include <bits/chrono.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace phlex::experimental;
using namespace oneapi::tbb;

TEST_CASE("Thread counter exception", "[multithreading]")
{
  thread_counter::counter_type counter{};
  CHECK_THROWS_WITH((thread_counter{counter, 0u}),
                    Catch::Matchers::ContainsSubstring("Too many threads encountered"));
}

TEST_CASE("Thread counter in flow graph", "[multithreading]")
{
  flow::graph g;
  unsigned int i{};
  flow::input_node src{g, [&i](flow_control& fc) -> unsigned int {
                         if (i < 10) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};
  thread_counter::counter_type unlimited_counter{};
  flow::function_node<unsigned int, unsigned int> unlimited_node{
    g, flow::unlimited, [&unlimited_counter](unsigned int const i) -> unsigned int {
      thread_counter c{unlimited_counter, -1u};
      sleep_for(5ms);
      return i;
    }};
  thread_counter::counter_type serial_counter{};
  flow::function_node<unsigned int, unsigned int> serial_node{
    g, flow::serial, [&serial_counter](unsigned int const i) -> unsigned int {
      thread_counter c{serial_counter};
      sleep_for(10ms);
      return i;
    }};
  thread_counter::counter_type max_counter{};
  flow::function_node<unsigned int, unsigned int> max_node{
    g, 4, [&max_counter](unsigned int const i) -> unsigned int {
      thread_counter c{max_counter, 4};
      sleep_for(10ms);
      return i;
    }};
  make_edge(src, unlimited_node);
  make_edge(src, serial_node);
  make_edge(src, max_node);
  src.activate();
  g.wait_for_all();
}
