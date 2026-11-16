#ifndef TEST_TBB_PREVIEW_MOCK_SIMULATION_HPP
#define TEST_TBB_PREVIEW_MOCK_SIMULATION_HPP

#include "phlex/core/resource/concepts.hpp"
#include "test/tbb-preview/dedicated_worker_pool.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace phlex::test {

  struct worker_lifecycle {
    std::thread::id initialized;
    std::thread::id executed;
    std::thread::id destroyed;
    unsigned int calls{};
  };

  class tracked_worker {
  public:
    worker_lifecycle& lifecycle;

    explicit tracked_worker(worker_lifecycle& record) : lifecycle{record}
    {
      lifecycle.initialized = std::this_thread::get_id();
    }
    tracked_worker(tracked_worker const&) = delete;
    tracked_worker(tracked_worker&&) = delete;
    tracked_worker& operator=(tracked_worker const&) = delete;
    tracked_worker& operator=(tracked_worker&&) = delete;
    ~tracked_worker() { lifecycle.destroyed = std::this_thread::get_id(); }
  };

  struct simulation_result {
    unsigned int value;
    std::thread::id worker_thread;
  };

  struct run_context {
    std::size_t number;
    unsigned int offset;
  };

  // Algorithms use typed operations; dispatch and worker access stay inside the resource.
  class mock_simulation {
    using pool_type = dedicated_worker_pool<std::function<tracked_worker()>>;

  public:
    class token_type {
    public:
      explicit token_type(pool_type::handle_type thread) : thread_{thread} {}

      simulation_result process_event(unsigned int value) const
      {
        return process_event(value, run_context{.number = 0, .offset = 0});
      }

      simulation_result process_event(unsigned int value, run_context const& run) const
      {
        return thread_->execute([value, &run](tracked_worker& worker) {
          auto& record = worker.lifecycle;
          record.executed = std::this_thread::get_id();
          ++record.calls;
          return simulation_result{.value = value + 1 + run.offset,
                                   .worker_thread = record.executed};
        });
      }

      void fail() const
      {
        thread_->execute([](tracked_worker& worker) {
          worker.lifecycle.executed = std::this_thread::get_id();
          throw std::runtime_error{"Exception from managed thread"};
        });
      }

    private:
      pool_type::handle_type thread_;
    };

    explicit mock_simulation(std::vector<worker_lifecycle>& records) :
      workers_{[&records, next = std::make_shared<std::size_t>(0)]() mutable {
                 return tracked_worker{records.at((*next)++)};
               },
               records.size()}
    {
      for (auto* thread : workers_.handles()) {
        tokens_.emplace_back(thread);
      }
    }

    std::span<token_type const> tokens() const { return tokens_; }

  private:
    pool_type workers_;
    std::vector<token_type> tokens_;
  };

  static_assert(phlex::detail::pooled_resource<mock_simulation>);

} // namespace phlex::test

#endif // TEST_TBB_PREVIEW_MOCK_SIMULATION_HPP
