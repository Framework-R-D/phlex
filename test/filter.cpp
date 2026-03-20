#include "phlex/core/framework_graph.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/concurrent_vector.h"

#include "spdlog/spdlog.h"

using namespace phlex;
using namespace oneapi::tbb;

namespace {
  class source {
  public:
    explicit source(unsigned const max_n) : max_{max_n} {}

    void operator()(framework_driver& driver)
    {
      auto job_index = driver.yield(data_cell_index::job());

      for (unsigned int i : std::views::iota(1u, max_ + 1)) {
        driver.yield(job_index->make_child("event", i));
      }
    }

  private:
    unsigned const max_;
  };

  experimental::fixed_hierarchy const flat_event_hierarchy{{{"event"}}};

  // Provider algorithms
  unsigned int give_me_nums(data_cell_index const& id) { return id.number() - 1; }

  unsigned int give_me_other_nums(data_cell_index const& id) { return 100 + id.number() - 1; }

  constexpr bool evens_only(unsigned int const value)
  {
    spdlog::debug("evens_only: {} is {}", value, value % 2 == 0 ? "even" : "odd");
    return value % 2u == 0u;
  }
  constexpr bool odds_only(unsigned int const value)
  {
    spdlog::debug("odds_only: {} is {}", value, value % 2 == 0 ? "even" : "odd");
    return not evens_only(value);
  }

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
      std::sort(begin(sorted_actual), end(sorted_actual));
      CHECK(expected == sorted_actual);
    }
    void collect(unsigned int const num) { actual.push_back(num); }
    tbb::concurrent_vector<unsigned int> actual;
    std::vector<unsigned int> const expected;
  };

  // Hacky!
  struct check_multiple_numbers {
    check_multiple_numbers(int const n) : total{n}
    {
      spdlog::debug("construct check_multiple_numbers with n = {}", n);
    }
    ~check_multiple_numbers() { CHECK(std::abs(sum) >= std::abs(total)); }
    void add_difference(unsigned int const a, unsigned int const b)
    {
      // The difference is calculated to test that add(a, b) yields a different result
      // than add(b, a).
      spdlog::debug(
        "check_multiple_numbers(n = {}): run add_difference. a = {}, b = {}", total, a, b);
      sum += static_cast<int>(b) - static_cast<int>(a);
    }
    std::atomic<int> sum;
    int const total;
  };

  constexpr bool in_range(unsigned int const b, unsigned int const e, unsigned int const i) noexcept
  {
    return i >= b and i < e;
  }

  struct not_in_range {
    explicit not_in_range(unsigned int const b, unsigned int const e) : begin{b}, end{e} {}
    unsigned int const begin;
    unsigned int const end;
    bool eval(unsigned int const i) const noexcept { return not in_range(begin, end, i); }
  };
}

TEST_CASE("Two predicates", "[filtering]")
{
  experimental::framework_graph g{source{10u}, flat_event_hierarchy};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.make<sum_numbers>(20u)
    .observe("add_evens", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when("evens_only");
  g.make<sum_numbers>(25u)
    .observe("add_odds", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when("odds_only");

  g.execute();

  CHECK(g.execution_count("add_evens") == 5);
  CHECK(g.execution_count("add_odds") == 5);
}

TEST_CASE("Two predicates in series", "[filtering]")
{
  experimental::framework_graph g{source{10u}, flat_event_hierarchy};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when("evens_only");
  g.make<sum_numbers>(0u)
    .observe("add", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when("odds_only");

  g.execute();

  CHECK(g.execution_count("add") == 0);
}

TEST_CASE("Two predicates in parallel", "[filtering]")
{
  experimental::framework_graph g{source{10u}, flat_event_hierarchy};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.make<sum_numbers>(0u)
    .observe("add", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when("odds_only", "evens_only");

  g.execute();

  CHECK(g.execution_count("add") == 0);
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

  experimental::framework_graph g{source{10u}, flat_event_hierarchy};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  for (auto const& [name, b, e] : configs) {
    g.make<not_in_range>(b, e)
      .predicate(name, &not_in_range::eval, concurrency::unlimited)
      .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  }

  std::vector<std::string> const predicate_names{
    "exclude_0_to_4", "exclude_6_to_7", "exclude_gt_8"};
  auto const expected_numbers = {4u, 5u, 7u};
  g.make<collect_numbers>(expected_numbers)
    .observe("collect", &collect_numbers::collect, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when(predicate_names);

  g.execute();

  CHECK(g.execution_count("collect") == 3);
}

TEST_CASE("Two predicates in parallel (each with multiple arguments)", "[filtering]")
{
  experimental::framework_graph g{source{10u}, flat_event_hierarchy};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.provide("provide_other_num", give_me_other_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "other_num"});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"});
  g.make<check_multiple_numbers>(5 * 100)
    .observe("check_evens", &check_multiple_numbers::add_difference, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "num"},
                  product_query{.creator = "input",
                                .layer = "event",
                                .suffix = "other_num"}) // <= Note input order
    .experimental_when("evens_only");

  g.make<check_multiple_numbers>(-5 * 100)
    .observe("check_odds", &check_multiple_numbers::add_difference, concurrency::unlimited)
    .input_family(
      product_query{.creator = "input", .layer = "event", .suffix = "other_num"},
      product_query{.creator = "input", .layer = "event", .suffix = "num"}) // <= Note input order
    .experimental_when("odds_only");
  g.observe(
     "print_evens",
     [](unsigned int i, unsigned int j) { spdlog::debug("{} is EVEN, j is {}", i, j); },
     concurrency::unlimited)
    .input_family(
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id},
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "other_num"_id})
    .experimental_when("evens_only");
  g.observe(
     "print_odds",
     [](unsigned int i, unsigned int j) { spdlog::debug("{} is ODD. j is {}", i, j); },
     concurrency::unlimited)
    .input_family(
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id},
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "other_num"_id})
    .experimental_when("odds_only");
  g.execute();

  CHECK(g.execution_count("check_odds") == 5);
  CHECK(g.execution_count("check_evens") == 5);
}
