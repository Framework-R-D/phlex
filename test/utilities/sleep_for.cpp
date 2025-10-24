#include "phlex/utilities/sleep_for.hpp"

#include "catch2/catch_test_macros.hpp"

#include <bits/chrono.h>
#include <chrono>

using namespace std::chrono;

TEST_CASE("Sleeping (1 s)", "[utilities]")
{
  auto start = steady_clock::now();
  phlex::experimental::sleep_for(1s);
  CHECK(steady_clock::now() - start >= 1s);
}

TEST_CASE("Spinning (1 s)", "[utilities]")
{
  auto start = steady_clock::now();
  phlex::experimental::spin_for(1s);
  CHECK(steady_clock::now() - start >= 1s);
}

TEST_CASE("Spinning (1000 ms)", "[utilities]")
{
  auto start = steady_clock::now();
  phlex::experimental::spin_for(1'000ms);
  CHECK(steady_clock::now() - start >= 1'000ms);
}
