#ifndef TEST_MOCK_WORKFLOW_TIMED_BUSY_HPP
#define TEST_MOCK_WORKFLOW_TIMED_BUSY_HPP

#include <chrono>

namespace phlex::experimental::test {
  void timed_busy(std::chrono::microseconds const& duration);
}

#endif // TEST_MOCK_WORKFLOW_TIMED_BUSY_HPP
