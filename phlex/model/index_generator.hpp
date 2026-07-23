#ifndef PHLEX_MODEL_INDEX_GENERATOR_HPP
#define PHLEX_MODEL_INDEX_GENERATOR_HPP

/// @file
/// @brief Provides a generator-like iterator for data_cell_index_ptr values.
///
/// This file exists to support generator-like behavior for compilers that do not necessarily
/// support std::generator (for example, Apple clang). It is intended to be removed once the
/// compilers we develop with support std::generator.
///
/// Phlex uses index_generator for sources that need to produce a sequence of data-cell index
/// pointers, typically for iterating over hierarchical data structures (e.g., run → subrun →
/// trigger record). The generator pattern allows sources to yield indices lazily as they traverse
/// the hierarchy.

#include "phlex/model/fwd.hpp"

#if defined(__has_include)
#if __has_include(<generator>)
#include <generator>
#include <version>
#endif
#endif

#if !defined(__cpp_lib_generator)
#include <coroutine>
#include <exception>
#include <iterator>
#include <memory>
#include <utility>
#endif

namespace phlex {
#if defined(__cpp_lib_generator)
  using index_generator = std::generator<data_cell_index_ptr>;
#else
  class index_generator {
  public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
      data_cell_index_ptr current_;
      std::exception_ptr exception_{};

      index_generator get_return_object()
      {
        return index_generator{handle_type::from_promise(*this)};
      }
      static std::suspend_always initial_suspend() noexcept { return {}; }
      static std::suspend_always final_suspend() noexcept { return {}; }
      std::suspend_always yield_value(data_cell_index_ptr value) noexcept
      {
        current_ = std::move(value);
        return {};
      }
      void return_void() noexcept {}
      void unhandled_exception() noexcept { exception_ = std::current_exception(); }
    };

    class iterator {
    public:
      using iterator_category = std::input_iterator_tag;
      using value_type = data_cell_index_ptr;
      using difference_type = std::ptrdiff_t;

      iterator() noexcept = default;
      explicit iterator(handle_type coroutine) noexcept : coroutine_{coroutine} {}

      iterator& operator++()
      {
        coroutine_.resume();
        if (coroutine_.done()) {
          auto& promise = coroutine_.promise();
          if (promise.exception_) {
            std::rethrow_exception(promise.exception_);
          }
        }
        return *this;
      }

      void operator++(int) { (void)++(*this); }

      value_type const& operator*() const noexcept { return coroutine_.promise().current_; }
      value_type const* operator->() const noexcept
      {
        return std::addressof(coroutine_.promise().current_);
      }

      bool operator==(std::default_sentinel_t) const noexcept
      {
        return !coroutine_ || coroutine_.done();
      }

    private:
      handle_type coroutine_;
    };

    index_generator() noexcept = default;

    explicit index_generator(handle_type coroutine) noexcept : coroutine_{coroutine} {}

    index_generator(index_generator const&) = delete;
    index_generator& operator=(index_generator const&) = delete;

    index_generator(index_generator&& other) noexcept :
      coroutine_{std::exchange(other.coroutine_, {})}
    {
    }

    index_generator& operator=(index_generator&& other) noexcept
    {
      if (this == &other) {
        return *this;
      }

      if (coroutine_) {
        coroutine_.destroy();
      }
      coroutine_ = std::exchange(other.coroutine_, {});
      return *this;
    }

    ~index_generator()
    {
      if (coroutine_) {
        coroutine_.destroy();
      }
    }

    iterator begin()
    {
      if (!coroutine_) {
        return iterator{};
      }

      coroutine_.resume();
      if (coroutine_.done()) {
        auto& promise = coroutine_.promise();
        if (promise.exception_) {
          std::rethrow_exception(promise.exception_);
        }
        return iterator{};
      }
      return iterator{coroutine_};
    }

    std::default_sentinel_t end() const noexcept { return {}; }

  private:
    handle_type coroutine_;
  };
#endif
}

#endif // PHLEX_MODEL_INDEX_GENERATOR_HPP
