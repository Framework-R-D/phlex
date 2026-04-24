#include "phlex/core/detail/accumulator_node.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_specification.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <map>
#include <memory>
#include <sstream>
#include <string>

using namespace oneapi;
using namespace phlex::experimental;

namespace {
  auto make_run_index(int run_number)
  {
    return phlex::data_cell_index::job()->make_child("run", run_number);
  }

  class message_collector : public tbb::flow::function_node<message> {
  public:
    explicit message_collector(tbb::flow::graph& g) :
      tbb::flow::function_node<message>{g, tbb::flow::serial, [this](message const& msg) {
                                          messages_.emplace(msg.store->index()->hash(), msg);
                                        }}
    {
    }

    std::size_t received_result_count() const { return messages_.size(); }

    std::pair<int, std::size_t> fold_result(phlex::data_cell_index_ptr const& idx) const
    {
      auto it = messages_.find(idx->hash());
      assert(it != messages_.end());
      return {it->second.store->get_product<int>(product_specification{}), it->second.id};
    }

  private:
    std::map<std::size_t, message> messages_;
  };

  void use_ostream_logger(std::ostringstream& oss)
  {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);
    auto ostream_logger = std::make_shared<spdlog::logger>("my_logger", ostream_sink);
    spdlog::set_default_logger(ostream_logger);
  }

  // Fold function: increments the accumulator by 1 per call. The accumulated value thus equals
  // the number of data cells processed.
  auto count_fold(detail::accumulator_message<int> const& msg) -> std::size_t
  {
    msg.partial_result->call([](int& acc) { acc += 1; });
    return msg.index->hash();
  }

  class accumulator_test_fixture {
  public:
    explicit accumulator_test_fixture(std::string node_name) :
      accumulator_{g_,
                   std::move(node_name),
                   "run"_id,
                   product_specifications(1),
                   [](phlex::data_cell_index const&) { return std::make_unique<int>(0); }},
      fold_op_{g_, tbb::flow::unlimited, count_fold},
      result_consumer_{g_}
    {
      make_edge(output_port<0>(accumulator_), result_consumer_);
      make_edge(output_port<1>(accumulator_), fold_op_);
      make_edge(fold_op_, input_port<3>(accumulator_));
    }

    void put_partition(index_message const& msg) { accumulator_.partition_port().try_put(msg); }
    void put_index_message(index_message const& msg) { accumulator_.index_port().try_put(msg); }
    void put_flush_token(indexed_end_token const& t) { accumulator_.flush_port().try_put(t); }

    void wait_for_all() { g_.wait_for_all(); }

    bool cache_is_empty() const { return accumulator_.cache_is_empty(); }
    std::size_t cache_size() const { return accumulator_.cache_size(); }
    std::size_t result_count() const { return result_consumer_.received_result_count(); }

    std::pair<int, std::size_t> fold_result(phlex::data_cell_index_ptr const& idx) const
    {
      return result_consumer_.fold_result(idx);
    }

  private:
    tbb::flow::graph g_;
    detail::accumulator_node<int> accumulator_;
    tbb::flow::function_node<detail::accumulator_message<int>, std::size_t> fold_op_;
    message_collector result_consumer_;
  };
}

TEST_CASE("Test accumulator basic functionality", "[multithreading]")
{
  auto partition_index = make_run_index(1);
  accumulator_test_fixture fixture{"test_accumulator_multiple_cells"};

  fixture.put_partition({.index = partition_index, .msg_id = 1});
  auto const num_data_cells = 5;
  for (std::size_t i = 0; i < num_data_cells; ++i) {
    fixture.put_index_message({.index = partition_index, .msg_id = 2 + i, .cache = true});
  }
  fixture.wait_for_all();

  // Fold is complete but no flush received — result should not be emitted yet
  CHECK(fixture.result_count() == 0);
  CHECK(fixture.cache_size() == 1);

  fixture.put_flush_token({.index = partition_index, .count = num_data_cells});
  fixture.wait_for_all();

  // Exactly one result carrying the accumulated count
  REQUIRE(fixture.result_count() == 1);
  CHECK(
    fixture.fold_result(partition_index) ==
    std::pair{num_data_cells, 1}); // Fold value equals number of data cells, original message ID 1
  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test accumulator with multiple partitions", "[multithreading]")
{
  auto run1 = make_run_index(1);
  auto run2 = make_run_index(2);
  accumulator_test_fixture fixture{"test_accumulator_multiple_partitions"};

  // Feed both partitions interleaved
  fixture.put_partition({.index = run1, .msg_id = 1});
  fixture.put_partition({.index = run2, .msg_id = 2});

  // 3 data cells for run1, 2 for run2, interleaved
  fixture.put_index_message({.index = run1, .msg_id = 3, .cache = true});
  fixture.put_index_message({.index = run2, .msg_id = 4, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 5, .cache = true});
  fixture.put_index_message({.index = run2, .msg_id = 6, .cache = true});
  fixture.put_index_message({.index = run1, .msg_id = 7, .cache = true});

  fixture.put_flush_token({.index = run1, .count = 3});
  fixture.put_flush_token({.index = run2, .count = 2});
  fixture.wait_for_all();

  // One result per partition: run1 accumulates 3 calls, run2 accumulates 2 calls
  REQUIRE(fixture.result_count() == 2);
  CHECK(fixture.fold_result(run1) == std::pair{3, 1}); // 3 fold operations, original message ID 1
  CHECK(fixture.fold_result(run2) == std::pair{2, 2}); // 2 fold operations, original message ID 2
  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test accumulator with index messages before partition", "[multithreading]")
{
  auto partition_index = make_run_index(1);
  accumulator_test_fixture fixture{"test_accumulator_index_before_partition"};

  // Send index messages before the partition message — they should be queued
  fixture.put_index_message({.index = partition_index, .msg_id = 1, .cache = true});
  fixture.put_index_message({.index = partition_index, .msg_id = 2, .cache = true});
  fixture.wait_for_all();

  // Cache entry exists but accumulator is not yet created, so no fold operations have run
  CHECK(fixture.result_count() == 0);
  CHECK(fixture.cache_size() == 1);

  // Partition arrives — pending index messages are dispatched and folds run
  fixture.put_partition({.index = partition_index, .msg_id = 3});
  fixture.wait_for_all();

  // Fold operations are done but no flush received yet
  CHECK(fixture.result_count() == 0);

  fixture.put_flush_token({.index = partition_index, .count = 2});
  fixture.wait_for_all();

  REQUIRE(fixture.result_count() == 1);
  CHECK(fixture.fold_result(partition_index) ==
        std::pair{2, 3}); // 2 fold operations, original message ID 3
  CHECK(fixture.cache_is_empty());
}

TEST_CASE("Test accumulator warning message if cache is not flushed", "[multithreading]")
{
  std::ostringstream oss;
  use_ostream_logger(oss);

  tbb::flow::graph g;
  auto accumulator = std::make_unique<detail::accumulator_node<int>>(
    g,
    "test_accumulator_warning",
    "run"_id,
    product_specifications(1),
    [](phlex::data_cell_index const&) { return std::make_unique<int>(0); });

  SECTION("Partition received, no flush")
  {
    accumulator->partition_port().try_put({.index = make_run_index(1), .msg_id = 1});
    g.wait_for_all();

    CHECK(accumulator->cache_size() == 1);

    accumulator.reset(); // Invoke destructor to trigger warning

    auto const warning = oss.str();
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached accumulators: 1"));
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Partition [run:1]"));
  }

  SECTION("Index message received, no partition")
  {
    accumulator->index_port().try_put({.index = make_run_index(1), .msg_id = 1, .cache = true});
    g.wait_for_all();

    CHECK(accumulator->cache_size() == 1);

    accumulator.reset(); // Invoke destructor to trigger warning

    auto const warning = oss.str();
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached accumulators: 1"));
    CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Partition index not yet received"));
  }
}

TEST_CASE("Test accumulator with zero data cells emits initial value", "[multithreading]")
{
  // This exercises the case where a partition exists but no data cells fall within it
  // (e.g. all events were filtered out). A flush with count=0 should immediately emit the
  // initial accumulator value without waiting for any fold operations.
  auto partition_index = make_run_index(1);
  accumulator_test_fixture fixture{"test_accumulator_zero_cells"};

  fixture.put_partition({.index = partition_index, .msg_id = 1});
  fixture.wait_for_all();

  CHECK(fixture.result_count() == 0);
  CHECK(fixture.cache_size() == 1);

  fixture.put_flush_token({.index = partition_index, .count = 0});
  fixture.wait_for_all();

  REQUIRE(fixture.result_count() == 1);
  CHECK(fixture.fold_result(partition_index) ==
        std::pair{0, 1}); // Initial value, original message ID 1
  CHECK(fixture.cache_is_empty());
}
