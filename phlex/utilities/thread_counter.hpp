#ifndef PHLEX_UTILITIES_THREAD_COUNTER_HPP
#define PHLEX_UTILITIES_THREAD_COUNTER_HPP

#include <atomic>
#include <stdexcept>
#include <string>

namespace phlex::experimental {
  class thread_counter {
  public:
    using counter_type = std::atomic<unsigned int>;
    using value_type = counter_type::value_type;
    thread_counter(counter_type& counter, value_type const max_value = 1) :
      counter_{counter}, max_{max_value}
    {
      auto const count = ++counter_;
      if (count > max_) {
        throw std::runtime_error("Too many threads encountered: " + std::to_string(count) +
                                 " vs. max allowed of " + std::to_string(max_));
      }
    }
    ~thread_counter() { --counter_; }

    // Copying and moving not intended
    thread_counter(thread_counter const&) = delete;
    thread_counter(thread_counter&&) = delete;

    thread_counter& operator=(thread_counter const&) = delete;
    thread_counter& operator=(thread_counter&&) = delete;

  private:
    counter_type& counter_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    value_type max_;
  };
}

#endif // PHLEX_UTILITIES_THREAD_COUNTER_HPP
