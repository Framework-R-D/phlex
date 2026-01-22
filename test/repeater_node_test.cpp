#include "phlex/core/detail/repeater_node.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <ranges>
#include <vector>

using namespace oneapi;
using namespace phlex;
using namespace phlex::experimental;

namespace {
  auto make_test_store(data_cell_index_ptr const& index, int value)
  {
    auto store = std::make_shared<product_store>(index);
    store->add_product("value", value);
    return store;
  }

  class message_collector : public tbb::flow::function_node<message> {
  public:
    explicit message_collector(tbb::flow::graph& g) :
      tbb::flow::function_node<message>{
        g, tbb::flow::serial, [this](message const& msg) { messages_.push_back(msg); }}
    {
    }

    auto const& messages() const noexcept { return messages_; }

    auto sorted_message_ids() const
    {
      auto ids = messages_ | std::views::transform([](auto const& msg) { return msg.id; }) |
                 std::ranges::to<std::vector<std::size_t>>();
      std::ranges::sort(ids);
      return ids;
    }

  private:
    std::vector<message> messages_;
  };

  void use_ostream_logger(std::ostringstream& oss)
  {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);
    auto ostream_logger = std::make_shared<spdlog::logger>("my_logger", ostream_sink);
    spdlog::set_default_logger(ostream_logger);
  }

  class repeater_test_fixture {
  public:
    repeater_test_fixture(std::string node_name, std::string layer_name = "run") :
      repeater_{g_, std::move(node_name), std::move(layer_name)}, consumer_{g_}
    {
      make_edge(repeater_, consumer_);
    }

    void put_data_message(message const& msg) { repeater_.data_port().try_put(msg); }
    void put_index_message(index_message const& msg) { repeater_.index_port().try_put(msg); }
    void put_flush_token(indexed_end_token const& token) { repeater_.flush_port().try_put(token); }

    void wait_for_all() { g_.wait_for_all(); }

    auto const& consumed_messages() const { return consumer_.messages(); }
    auto sorted_message_ids() const { return consumer_.sorted_message_ids(); }

  private:
    tbb::flow::graph g_;
    detail::repeater_node repeater_;
    message_collector consumer_;
  };
}

TEST_CASE("Test repeater pass-through mode", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_pass_through"};

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto store1 = make_test_store(run1, 42);

  // Send index message with cache=false (pass-through mode)
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = false});

  // Send data message
  fixture.put_data_message({.store = store1, .id = 0});

  fixture.wait_for_all();

  // In pass-through mode, message should be received immediately
  REQUIRE(fixture.consumed_messages().size() == 1);
  CHECK(fixture.consumed_messages()[0].store == store1);
  CHECK(fixture.consumed_messages()[0].id == 0);
}

TEST_CASE("Test repeater caching mode", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_caching_mode"};

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto store1 = make_test_store(run1, 42);

  // Send index messages first (with cache=true, which is the default)
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 2, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 3, .cache = true});

  fixture.wait_for_all();

  // No messages yet, waiting for data
  CHECK(fixture.consumed_messages().empty());

  // Send data message - should trigger emission of all queued message IDs
  fixture.put_data_message({.store = store1, .id = 0});

  fixture.wait_for_all();

  // Should receive 3 messages with the same store but different IDs
  REQUIRE(fixture.consumed_messages().size() == 3);
  // The messages are not guaranteed to arrive at a particular order, so we sort by message ID
  // before checking.
  CHECK(fixture.sorted_message_ids() == std::vector<std::size_t>{1, 2, 3});
  CHECK(fixture.consumed_messages()[0].store == store1);
  CHECK(fixture.consumed_messages()[1].store == store1);
  CHECK(fixture.consumed_messages()[2].store == store1);

  // Now evict the data (we actually test flushing in the next test, but we do this to avoid
  // any warnings about cached products not being flushed)
  fixture.put_flush_token({.index = run1, .count = 3});
  fixture.wait_for_all();
}

TEST_CASE("Test repeater with flush tokens", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_with_flush_tokens"};

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto store1 = make_test_store(run1, 42);

  // Send index messages
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 2, .cache = true});

  // Send data message
  fixture.put_data_message({.store = store1, .id = 0});

  fixture.wait_for_all();

  REQUIRE(fixture.consumed_messages().size() == 2);

  // Send flush token with count=2 (indicating 2 messages were sent)
  fixture.put_flush_token({.index = run1, .count = 2});

  fixture.wait_for_all();

  // Cache should be cleaned up after flush with counter reaching zero
  CHECK(fixture.consumed_messages().size() == 2);
}

TEST_CASE("Test repeater multiple indices", "[multithreading]")
{
  tbb::flow::graph g;
  detail::repeater_node repeater{g, "test_repeater_multiple_indices", "run"};

  std::atomic<unsigned> message_count{0};
  tbb::flow::function_node<message> consumer{
    g, tbb::flow::unlimited, [&message_count](message const&) { ++message_count; }};

  make_edge(repeater, consumer);

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto run2 = data_cell_index::base_ptr()->make_child(2, "run");
  auto store1 = make_test_store(run1, 42);
  auto store2 = make_test_store(run2, 43);

  // Send index messages for different runs
  repeater.index_port().try_put({.index = run1, .msg_id = 1, .cache = true});
  repeater.index_port().try_put({.index = run2, .msg_id = 2, .cache = true});
  repeater.index_port().try_put({.index = run1, .msg_id = 3, .cache = true});

  // Send data for run1
  repeater.data_port().try_put({.store = store1, .id = 0});

  g.wait_for_all();

  // Should receive 2 messages for run1 (msg_id 1 and 3)
  CHECK(message_count == 2);

  // Send data for run2
  repeater.data_port().try_put({.store = store2, .id = 0});

  g.wait_for_all();

  // Should now have 3 total messages
  CHECK(message_count == 3);

  // Send flush tokens
  repeater.flush_port().try_put({.index = run1, .count = 2});
  repeater.flush_port().try_put({.index = run2, .count = 1});

  g.wait_for_all();
}

TEST_CASE("Test repeater transition to pass-through", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_transition_to_pass_through"};

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto store1 = make_test_store(run1, 42);

  // Start in caching mode
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = true});

  // Send data
  fixture.put_data_message({.store = store1, .id = 0});

  fixture.wait_for_all();

  CHECK(fixture.consumed_messages().size() == 1);

  // Transition to pass-through mode with cache=false
  fixture.put_index_message({.index = run1, .msg_id = 2, .cache = false});

  fixture.wait_for_all();

  // The cached product should be output during transition
  CHECK(fixture.consumed_messages().size() == 2);
  CHECK(fixture.consumed_messages()[1].store == store1);

  // Now in pass-through mode, subsequent data should go through directly
  auto store2 = make_test_store(run1, 99);
  fixture.put_data_message({.store = store2, .id = 0});

  fixture.wait_for_all();

  CHECK(fixture.consumed_messages().size() == 3);
  CHECK(fixture.consumed_messages()[2].store == store2);
}

TEST_CASE("Test repeater data before index", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_data_before_index"};

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto store1 = make_test_store(run1, 42);

  // Send data first (before any index messages)
  fixture.put_data_message({.store = store1, .id = 0});

  fixture.wait_for_all();

  // No messages yet
  CHECK(fixture.consumed_messages().empty());

  // Now send index messages
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 2, .cache = true});

  fixture.wait_for_all();

  // Should receive messages now with the cached data
  REQUIRE(fixture.consumed_messages().size() == 2);
  // The messages are not guaranteed to arrive at a particular order, so we sort by message ID
  // before checking.
  CHECK(fixture.sorted_message_ids() == std::vector<std::size_t>{1, 2});
  CHECK(fixture.consumed_messages()[0].store == store1);
  CHECK(fixture.consumed_messages()[1].store == store1);

  // Now evict the data (we actually test flushing in an earlier test, but we do this to avoid
  // any warnings about cached products not being flushed)
  fixture.put_flush_token({.index = run1, .count = 2});
  fixture.wait_for_all();
}

TEST_CASE("Test warning message if there are cached products", "[multithreading]")
{
  // Setup ostream sink to capture warning message
  std::ostringstream oss;
  use_ostream_logger(oss);

  // Below, we want to trigger the destruction of the repeater, which will emit a warning if
  // there are still cached products that were never flushed. To do this without introducing a
  // new scope, we create the fixture as a unique_ptr and then reset it at the end of the test,
  // which will invoke the destructor.
  auto fixture =
    std::make_unique<repeater_test_fixture>("test_repeater_warning_on_cached_products");

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto store1 = make_test_store(run1, 42);

  // Send index message with cache=true (caching mode)
  fixture->put_index_message({.index = run1, .msg_id = 1, .cache = true});
  fixture->put_data_message({.store = store1, .id = 0});

  fixture->wait_for_all();

  REQUIRE(fixture->consumed_messages().size() == 1);

  fixture.reset(); // Invoke fixture destructor to trigger warning message

  auto const warning = oss.str();
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached messages: 1"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Product for [run:1]"));
}

TEST_CASE("Test warning message if there are cached messages", "[multithreading]")
{
  // Setup ostream sink to capture warning message
  std::ostringstream oss;
  use_ostream_logger(oss);

  // Below, we want to trigger the destruction of the repeater, which will emit a warning if
  // there are still cached messages that were never flushed. To do this without introducing a
  // new scope, we create the fixture as a unique_ptr and then reset it at the end of the test,
  // which will invoke the destructor.
  auto fixture =
    std::make_unique<repeater_test_fixture>("test_repeater_warning_on_cached_messages");

  auto run1 = data_cell_index::base_ptr()->make_child(1, "run");
  auto store1 = make_test_store(run1, 42);

  // Send index message with cache=true (caching mode)
  fixture->put_index_message({.index = run1, .msg_id = 1, .cache = true});

  fixture->wait_for_all();

  REQUIRE(
    fixture->consumed_messages().empty()); // No messages yet since data message hasn't been sent

  fixture.reset(); // Invoke fixture destructor to trigger warning message

  auto const warning = oss.str();
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached messages: 1"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Product not yet received"));
}
