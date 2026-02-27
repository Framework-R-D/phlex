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
#include <utility>
#include <vector>

using namespace oneapi;
using namespace phlex::experimental;

namespace {
  auto make_run_index(int run_number)
  {
    return phlex::data_cell_index::base_ptr()->make_child(run_number, "run");
  }

  auto make_run_with_product(int run_number, int value)
  {
    auto index = make_run_index(run_number);
    auto store = std::make_shared<product_store>(index);
    store->add_product("value", value);
    return std::pair{index, store};
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

    auto sorted_messages() const
    {
      auto msgs = messages_;
      std::ranges::sort(msgs, {}, &message::id);
      return msgs;
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
    explicit repeater_test_fixture(std::string node_name) :
      repeater_{g_, std::move(node_name), "run"_id}, consumer_{g_}
    {
      make_edge(repeater_, consumer_);
    }

    void put_data_message(message const& msg) { repeater_.data_port().try_put(msg); }
    void put_index_message(index_message const& msg) { repeater_.index_port().try_put(msg); }
    void put_flush_token(indexed_end_token const& token) { repeater_.flush_port().try_put(token); }

    void wait_for_all() { g_.wait_for_all(); }

    bool cache_is_empty() const { return repeater_.cache_is_empty(); }
    std::size_t cache_size() const { return repeater_.cache_size(); }

    auto const& consumed_messages() const { return consumer_.messages(); }
    auto sorted_message_ids() const { return consumer_.sorted_message_ids(); }
    auto sorted_messages() const { return consumer_.sorted_messages(); }

  private:
    tbb::flow::graph g_;
    detail::repeater_node repeater_;
    message_collector consumer_;
  };
}

TEST_CASE("Test repeater pass-through mode", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_pass_through"};
  auto [run1, store1] = make_run_with_product(1, 42);

  // Send index message with cache=false (pass-through mode), followed by data message
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = false});
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
  auto [run1, store1] = make_run_with_product(1, 42);

  // Send index messages first (with cache=true, which is the default)
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 2, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 3, .cache = true});
  fixture.wait_for_all();

  // No messages yet, waiting for data
  CHECK(fixture.consumed_messages().empty());
  // There is one cache entry corresponding to the one index (run1) that we've sent messages for
  CHECK(fixture.cache_size() == 1);

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

  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test repeater with flush tokens", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_with_flush_tokens"};
  auto [run1, store1] = make_run_with_product(1, 42);

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
  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test repeater multiple indices", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_multiple_indices"};
  auto [run1, store1] = make_run_with_product(1, 42);
  auto [run2, store2] = make_run_with_product(2, 43);

  // Send index messages for two different runs — run1 gets msg_ids 1 and 3, run2 gets msg_id 2.
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = true});
  fixture.put_index_message({.index = run2, .msg_id = 2, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 3, .cache = true});

  // Sending store1 (keyed to run1) should release the two run1 index messages.
  fixture.put_data_message({.store = store1, .id = 0});
  fixture.wait_for_all();

  // Should receive exactly 2 messages, both carrying store1.
  // The messages are not guaranteed to arrive in a particular order, so we sort by message ID
  // before checking.
  REQUIRE(fixture.consumed_messages().size() == 2);
  CHECK(fixture.sorted_message_ids() == std::vector<std::size_t>{1, 3});
  for (auto const& msg : fixture.consumed_messages()) {
    CHECK(msg.store == store1);
  }

  // Sending store2 (keyed to run2) should release the one run2 index message.
  fixture.put_data_message({.store = store2, .id = 0});
  fixture.wait_for_all();

  // Should now have 3 total messages.  Sort by ID so we can assert stores in a defined order.
  REQUIRE(fixture.consumed_messages().size() == 3);
  auto const sorted = fixture.sorted_messages();
  CHECK(sorted[0].id == 1);
  CHECK(sorted[0].store == store1);
  CHECK(sorted[1].id == 2);
  CHECK(sorted[1].store == store2);
  CHECK(sorted[2].id == 3);
  CHECK(sorted[2].store == store1);

  // Flush both runs to clear the cache.
  fixture.put_flush_token({.index = run1, .count = 2});
  fixture.put_flush_token({.index = run2, .count = 1});
  fixture.wait_for_all();

  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test repeater transition to pass-through", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_transition_to_pass_through"};
  auto [run1, store1] = make_run_with_product(1, 42);

  // The repeater_node starts in caching mode by default.  So we send a data message first, which
  // will be cached.
  fixture.put_data_message({.store = store1, .id = 0});
  fixture.wait_for_all();

  CHECK(fixture.cache_size() == 1);

  // Now send an index message with cache=false, which should trigger an immediate transition to
  // pass-through mode.
  fixture.put_index_message({.index = run1, .msg_id = 1, .cache = false});
  fixture.wait_for_all();

  CHECK(fixture.consumed_messages().size() == 1);
  CHECK(fixture.consumed_messages()[0].store == store1);
  // Cache should be cleared immediately upon transition to pass-through mode.
  CHECK(fixture.cache_is_empty());

  // Now in pass-through mode, subsequent data should go through directly
  auto [_, store2] = make_run_with_product(1, 99);
  fixture.put_data_message({.store = store2, .id = 0});
  fixture.wait_for_all();

  CHECK(fixture.consumed_messages().size() == 2);
  CHECK(fixture.consumed_messages()[1].store == store2);
  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test repeater data before index", "[multithreading]")
{
  repeater_test_fixture fixture{"test_repeater_data_before_index"};
  auto [run1, store1] = make_run_with_product(1, 42);

  // Send data first (before any index messages)
  fixture.put_data_message({.store = store1, .id = 0});
  fixture.wait_for_all();

  // No messages yet
  CHECK(fixture.consumed_messages().empty());
  CHECK(fixture.cache_size() == 1);

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

  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test warning message if there are cached messages", "[multithreading]")
{
  // Setup ostream sink to capture warning message
  std::ostringstream oss;
  use_ostream_logger(oss);

  // Below, we want to trigger the destruction of the repeater, which will emit a warning if
  // there are still cached messages that were never flushed. To do this without introducing a
  // new scope, we create the repeater_nopde as a unique_ptr and then reset it at the end of
  // the test, which will invoke the destructor.
  tbb::flow::graph g;
  auto repeater = std::make_unique<detail::repeater_node>(
    g, "test_repeater_warning_on_cached_messages", "run"_id);

  SECTION("Cached data product")
  {
    // Send data message only
    auto [_, store1] = make_run_with_product(1, 42);
    repeater->data_port().try_put({.store = store1, .id = 0});
    g.wait_for_all();

    CHECK(repeater->cache_size() == 1);

    repeater.reset(); // Invoke repeater destructor to trigger warning message

    auto const warning = oss.str();
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached messages: 1"));
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Product for [run:1]"));
  }

  SECTION("Cached message IDs")
  {
    // Send index message only
    repeater->index_port().try_put({.index = make_run_index(1), .msg_id = 1, .cache = true});
    g.wait_for_all();

    CHECK(repeater->cache_size() == 1);

    repeater.reset(); // Invoke repeater destructor to trigger warning message

    auto const warning = oss.str();
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached messages: 1"));
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Product not yet received"));
  }
}
