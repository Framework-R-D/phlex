// =======================================================================================
// This test executes unfoldting functionality using the following graph
//
//     Multiplexer
//          |
//      unfold (creates children)
//          |
//         add(*)
//          |
//     print_result
//
// where the asterisk (*) indicates a fold step.  The difference here is that the
// *unfold* is responsible for sending the flush token instead of the
// source/multiplexer.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "test/products_for_output.hpp"

#include "catch2/catch_all.hpp"

#include <atomic>
#include <ranges>
#include <string>

using namespace phlex::experimental;

namespace {
  class iota {
  public:
    explicit iota(unsigned int max_number) : max_{max_number} {}
    unsigned int initial_value() const { return 0; }
    bool predicate(unsigned int i) const { return i != max_; }
    auto unfold(unsigned int i) const { return std::make_pair(i + 1, i); };

  private:
    unsigned int max_;
  };

  using numbers_t = std::vector<unsigned int>;

  class iterate_through {
  public:
    explicit iterate_through(numbers_t const& numbers) :
      begin_{numbers.begin()}, end_{numbers.end()}
    {
    }
    auto initial_value() const { return begin_; }
    bool predicate(numbers_t::const_iterator it) const { return it != end_; }
    auto unfold(numbers_t::const_iterator it, level_id const& lid) const
    {
      spdlog::info("Unfolding into {}", lid.to_string());
      auto num = *it;
      return std::make_pair(++it, num);
    };

  private:
    numbers_t::const_iterator begin_;
    numbers_t::const_iterator end_;
  };

  void add(std::atomic<unsigned int>& counter, unsigned number) { counter += number; }
  void add_numbers(std::atomic<unsigned int>& counter, unsigned number) { counter += number; }

  void check_sum(handle<unsigned int> const sum)
  {
    if (sum.level_id().number() == 0ull) {
      CHECK(*sum == 45);
    } else {
      CHECK(*sum == 190);
    }
  }

  void check_sum_same(handle<unsigned int> const sum)
  {
    auto const expected_sum = (sum.level_id().number() + 1) * 10;
    CHECK(*sum == expected_sum);
  }
}

TEST_CASE("Splitting the processing", "[graph]")
{
  constexpr auto index_limit = 2u;

  auto levels_to_process = [index_limit](auto& driver) {
    auto job_store = product_store::base();
    driver.yield(job_store);
    for (unsigned i : std::views::iota(0u, index_limit)) {
      auto event_store = job_store->make_child(i, "event");
      event_store->add_product<unsigned>("max_number", 10u * (i + 1));
      event_store->add_product<numbers_t>("ten_numbers", numbers_t(10, i + 1));
      driver.yield(event_store);
    }
  };

  framework_graph g{levels_to_process};

  g.with<iota>(&iota::predicate, &iota::unfold, concurrency::unlimited)
    .unfold("max_number")
    .into("new_number")
    .within_family("lower1");
  g.with("add", add, concurrency::unlimited).fold("new_number").partitioned_by("event").to("sum1");
  g.with("check_sum", check_sum, concurrency::unlimited).observe("sum1");

  g.with<iterate_through>(
     &iterate_through::predicate, &iterate_through::unfold, concurrency::unlimited)
    .unfold("ten_numbers")
    .into("each_number")
    .within_family("lower2");
  g.with("add_numbers", add_numbers, concurrency::unlimited)
    .fold("each_number")
    .partitioned_by("event")
    .to("sum2");
  g.with("check_sum_same", check_sum_same, concurrency::unlimited).observe("sum2");

  g.make<test::products_for_output>().output_with(
    "save", &test::products_for_output::save, concurrency::serial);

  g.execute("unfold_t");

  CHECK(g.execution_counts("iota") == index_limit);
  CHECK(g.execution_counts("add") == 30);
  CHECK(g.execution_counts("check_sum") == index_limit);

  CHECK(g.execution_counts("iterate_through") == index_limit);
  CHECK(g.execution_counts("add_numbers") == 20);
  CHECK(g.execution_counts("check_sum_same") == index_limit);
}
