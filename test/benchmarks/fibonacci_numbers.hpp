#ifndef TEST_BENCHMARKS_FIBONACCI_NUMBERS_HPP
#define TEST_BENCHMARKS_FIBONACCI_NUMBERS_HPP

#include <vector>

namespace test {
  class fibonacci_numbers {
  public:
    explicit fibonacci_numbers(int n);
    bool accept(int i) const;

  private:
    std::vector<int> numbers_;
  };
}

#endif // TEST_BENCHMARKS_FIBONACCI_NUMBERS_HPP
