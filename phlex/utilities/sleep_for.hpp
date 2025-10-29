#ifndef PHLEX_UTILITIES_SLEEP_FOR_HPP
#define PHLEX_UTILITIES_SLEEP_FOR_HPP

#include <chrono>
#include <thread>

namespace phlex::experimental {
  template <typename T>
  void sleep_for(T duration)
  {
    std::this_thread::sleep_for(duration);
  }

  template <typename T>
  void spin_for(T duration)
  {
    using namespace std::chrono;
    auto start = steady_clock::now();
    while (duration_cast<T>(steady_clock::now() - start) < duration) {}
  }

  // Risky...
  using namespace std::chrono_literals;
}

#endif // PHLEX_UTILITIES_SLEEP_FOR_HPP
