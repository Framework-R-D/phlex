#include "test/mock-workflow/timed_busy.hpp"
#include <chrono>

void phlex::experimental::test::timed_busy(std::chrono::microseconds const& duration)
{
  using namespace std::chrono;
  auto const stop = steady_clock::now() + duration;
  while (steady_clock::now() < stop) {
    // Do nothing
  }
}
