#include "test/benchmarks/fibonacci_numbers.hpp"

#include <algorithm>

namespace {
  auto fibs_less_than(int const n)
  {
    std::vector<int> result;
    int i = 0;
    int j = 1;
    int sum = 0;
    while (sum < n) {
      result.push_back(sum);
      sum = i + j;
      i = j;
      j = sum;
    }
    return result;
  }
}

namespace test {
  fibonacci_numbers::fibonacci_numbers(int const n) : numbers_{fibs_less_than(n + 1)} {}

  bool fibonacci_numbers::accept(int i) const
  {
    return std::binary_search(begin(numbers_), end(numbers_), i);
  }
}
