#include "phlex/utilities/thread_counter.hpp"

#include "catch2/catch_all.hpp"
#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <vector>

using namespace phlex::experimental;
using namespace oneapi::tbb;

namespace {

  std::atomic<unsigned int> processed_messages{};
  struct thread_unsafe {
    std::atomic<unsigned int> counter{};

    auto increment(unsigned int i)
    {
      thread_counter c{counter};
      ++processed_messages;
      return i;
    }
  };

  template <typename Input>
  using replicated_node_base = tbb::flow::composite_node<std::tuple<Input>, std::tuple<Input>>;

  template <typename T, typename Input>
  class replicated_node : public replicated_node_base<Input> {
    using base_t = replicated_node_base<Input>;

  public:
    template <typename R, typename... Args>
    replicated_node(tbb::flow::graph& g, std::size_t n, R (T::*ft)(Args...)) :
      replicated_node_base<Input>{g}, buffer_{g}, broadcast_{g}, modules_(n)
    {
      nodes_.reserve(n); // N.B. TBB uses the address of the nodes, so an emplace_back w/o
                         // a reserve will invalidate anything that's already cached.
      for (auto& module : modules_) {
        auto& node = nodes_.emplace_back(
          g, flow::serial, [&module, ft](unsigned int i) { return (module.*ft)(i); });
        make_edge(buffer_, node);
        make_edge(node, broadcast_);
      }

      base_t::set_external_ports(typename base_t::input_ports_type{buffer_},
                                 typename base_t::output_ports_type{broadcast_});
    }

  public:
    tbb::flow::buffer_node<Input> buffer_;
    tbb::flow::broadcast_node<Input> broadcast_;
    std::vector<T> modules_;
    std::vector<tbb::flow::function_node<Input, Input, tbb::flow::rejecting>> nodes_;
  };
}

TEST_CASE("Replicated function calls", "[multithreading]")
{
  constexpr unsigned int total_messages{10u};
  flow::graph g;
  flow::input_node src{g, [i = 0u](flow_control& fc) mutable {
                         if (i < total_messages) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};

  replicated_node<thread_unsafe, unsigned int> replicated{g, 4, &thread_unsafe::increment};
  make_edge(src, replicated);

  src.activate();
  g.wait_for_all();

  CHECK(processed_messages == total_messages);
}
