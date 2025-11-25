#include "phlex/core/framework_graph.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/concurrent_vector.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <vector>

using namespace phlex::experimental;
using namespace oneapi::tbb;

namespace {
  class source {
  public:
    explicit source(unsigned const max_n) : max_{max_n} {}

    void operator()(framework_driver& driver)
    {
      auto job_store = product_store::base();
      driver.yield(job_store);

      for (unsigned int i : std::views::iota(1u, max_ + 1)) {
        auto store = job_store->make_child(i, "event");
        store->add_product<unsigned int>("num", i - 1);
        store->add_product<unsigned int>("other_num", 100 + i - 1);
        driver.yield(store);
      }
    }

  private:
    unsigned const max_;
  };

  constexpr auto evens_only(unsigned int const value) -> bool { return value % 2u == 0u; }
  constexpr auto odds_only(unsigned int const value) -> bool { return not evens_only(value); }

  // Hacky!
  struct sum_numbers {
    sum_numbers(unsigned int const n) : total{n} {}
    ~sum_numbers() { CHECK(sum == total); }
    void add(unsigned int const num) { sum += num; }
    std::atomic<unsigned int> sum;
    unsigned int const total;
  };

  // Hacky!
  struct collect_numbers {
    collect_numbers(std::initializer_list<unsigned int> numbers) : expected{numbers} {}
    ~collect_numbers()
    {
      std::vector<unsigned int> sorted_actual(std::begin(actual), std::end(actual));
      std::ranges::sort(sorted_actual);
      CHECK(expected == sorted_actual);
    }
    void collect(unsigned int const num) { actual.push_back(num); }
    tbb::concurrent_vector<unsigned int> actual;
    std::vector<unsigned int> const expected;
  };

  // Hacky!
  struct check_multiple_numbers {
    check_multiple_numbers(int const n) : total{n} {}
    ~check_multiple_numbers() { CHECK(std::abs(sum) >= std::abs(total)); }
    void add_difference(unsigned int const a, unsigned int const b)
    {
      // The difference is calculated to test that add(a, b) yields a different result
      // than add(b, a).
      sum += static_cast<int>(b) - static_cast<int>(a);
    }
    std::atomic<int> sum;
    int const total;
  };

  constexpr auto in_range(unsigned int const b, unsigned int const e, unsigned int const i) noexcept -> bool
  {
    return i >= b and i < e;
  }

  struct not_in_range {
    explicit not_in_range(unsigned int const b, unsigned int const e) : begin{b}, end{e} {}
    unsigned int const begin;
    unsigned int const end;
    [[nodiscard]] auto eval(unsigned int const i) const noexcept -> bool { return not in_range(begin, end, i); }
  };
}

TEST_CASE("Two predicates", "[filtering]")
{
  framework_graph g{source{10u}};
  g.predicate("evens_only", evens_only, concurrency::unlimited).input_family("num"_in("event"));
  g.predicate("odds_only", odds_only, concurrency::unlimited).input_family("num"_in("event"));
  g.make<sum_numbers>(20u)
    .observe("add_evens", &sum_numbers::add, concurrency::unlimited)
    .input_family("num"_in("event"))
    .when("evens_only");
  g.make<sum_numbers>(25u)
    .observe("add_odds", &sum_numbers::add, concurrency::unlimited)
    .input_family("num"_in("event"))
    .when("odds_only");

  g.execute();

  CHECK(g.execution_counts("add_evens") == 5);
  CHECK(g.execution_counts("add_odds") == 5);
}

TEST_CASE("Two predicates in series", "[filtering]")
{
  framework_graph g{source{10u}};
  g.predicate("evens_only", evens_only, concurrency::unlimited).input_family("num");
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family("num")
    .when("evens_only");
  g.make<sum_numbers>(0u)
    .observe("add", &sum_numbers::add, concurrency::unlimited)
    .input_family("num")
    .when("odds_only");

  g.execute();

  CHECK(g.execution_counts("add") == 0);
}

TEST_CASE("Two predicates in parallel", "[filtering]")
{
  framework_graph g{source{10u}};
  g.predicate("evens_only", evens_only, concurrency::unlimited).input_family("num");
  g.predicate("odds_only", odds_only, concurrency::unlimited).input_family("num");
  g.make<sum_numbers>(0u)
    .observe("add", &sum_numbers::add, concurrency::unlimited)
    .input_family("num")
    .when("odds_only", "evens_only");

  g.execute();

  CHECK(g.execution_counts("add") == 0);
}

TEST_CASE("Three predicates in parallel", "[filtering]")
{
  struct predicate_config {
    std::string name;
    unsigned int begin;
    unsigned int end;
  };
  std::vector<predicate_config> configs{{.name = "exclude_0_to_4", .begin = 0, .end = 4},
                                        {.name = "exclude_6_to_7", .begin = 6, .end = 7},
                                        {.name = "exclude_gt_8", .begin = 8, .end = -1u}};

  framework_graph g{source{10u}};
  for (auto const& [name, b, e] : configs) {
    g.make<not_in_range>(b, e)
      .predicate(name, &not_in_range::eval, concurrency::unlimited)
      .input_family("num");
  }

  std::vector<std::string> const predicate_names{
    "exclude_0_to_4", "exclude_6_to_7", "exclude_gt_8"};
  auto const expected_numbers = {4u, 5u, 7u};
  g.make<collect_numbers>(expected_numbers)
    .observe("collect", &collect_numbers::collect, concurrency::unlimited)
    .input_family("num")
    .when(predicate_names);

  g.execute();

  CHECK(g.execution_counts("collect") == 3);
}

TEST_CASE("Two predicates in parallel (each with multiple arguments)", "[filtering]")
{
  framework_graph g{source{10u}};
  g.predicate("evens_only", evens_only, concurrency::unlimited).input_family("num");
  g.predicate("odds_only", odds_only, concurrency::unlimited).input_family("num");
  g.make<check_multiple_numbers>(5 * 100)
    .observe("check_evens", &check_multiple_numbers::add_difference, concurrency::unlimited)
    .input_family("num", "other_num") // <= Note input order
    .when("evens_only");

  g.make<check_multiple_numbers>(-5 * 100)
    .observe("check_odds", &check_multiple_numbers::add_difference, concurrency::unlimited)
    .input_family("other_num", "num") // <= Note input order
    .when("odds_only");

  g.execute();

  CHECK(g.execution_counts("check_odds") == 5);
  CHECK(g.execution_counts("check_evens") == 5);
}
