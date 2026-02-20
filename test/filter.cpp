#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/concurrent_vector.h"

using namespace phlex;
using namespace oneapi::tbb;

namespace {
  class source {
  public:
    explicit source(unsigned const max_n) : max_{max_n} {}

    void operator()(framework_driver& driver)
    {
      auto job_index = data_cell_index::base_ptr();
      driver.yield(job_index);

      for (unsigned int i : std::views::iota(1u, max_ + 1)) {
        auto index = job_index->make_child(i, "event");
        driver.yield(index);
      }
    }

  private:
    unsigned const max_;
  };

  // Provider algorithms
  unsigned int give_me_nums(data_cell_index const& id) { return id.number() - 1; }

  unsigned int give_me_other_nums(data_cell_index const& id) { return 100 + id.number() - 1; }

  constexpr bool evens_only(unsigned int const value) { return value % 2u == 0u; }
  constexpr bool odds_only(unsigned int const value) { return not evens_only(value); }

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
  experimental::framework_graph g{source{10u}};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.make<sum_numbers>(20u)
    .observe("add_evens", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id})
    .experimental_when("evens_only");
  g.make<sum_numbers>(25u)
    .observe("add_odds", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id})
    .experimental_when("odds_only");

  g.execute();

  CHECK(g.execution_count("add_evens") == 5);
  CHECK(g.execution_count("add_odds") == 5);
}

TEST_CASE("Two predicates in series", "[filtering]")
{
  experimental::framework_graph g{source{10u}};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id})
    .experimental_when("evens_only");
  g.make<sum_numbers>(0u)
    .observe("add", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id})
    .experimental_when("odds_only");

  g.execute();

  CHECK(g.execution_count("add") == 0);
}

TEST_CASE("Two predicates in parallel", "[filtering]")
{
  experimental::framework_graph g{source{10u}};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.make<sum_numbers>(0u)
    .observe("add", &sum_numbers::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id})
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

  experimental::framework_graph g{source{10u}};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  for (auto const& [name, b, e] : configs) {
    g.make<not_in_range>(b, e)
      .predicate(name, &not_in_range::eval, concurrency::unlimited)
      .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  }

  std::vector<std::string> const predicate_names{
    "exclude_0_to_4", "exclude_6_to_7", "exclude_gt_8"};
  auto const expected_numbers = {4u, 5u, 7u};
  g.make<collect_numbers>(expected_numbers)
    .observe("collect", &collect_numbers::collect, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id})
    .experimental_when(predicate_names);

  g.execute();

  CHECK(g.execution_count("collect") == 3);
}

TEST_CASE("Two predicates in parallel (each with multiple arguments)", "[filtering]")
{
  experimental::framework_graph g{source{10u}};
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.provide("provide_other_num", give_me_other_nums, concurrency::unlimited)
    .output_product(
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "other_num"_id});
  g.predicate("evens_only", evens_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.predicate("odds_only", odds_only, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id});
  g.make<check_multiple_numbers>(5 * 100)
    .observe("check_evens", &check_multiple_numbers::add_difference, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "num"_id},
                  product_query{.creator = "input"_id,
                                .layer = "event"_id,
                                .suffix = "other_num"_id}) // <= Note input order
    .experimental_when("evens_only");

  g.make<check_multiple_numbers>(-5 * 100)
    .observe("check_odds", &check_multiple_numbers::add_difference, concurrency::unlimited)
    .input_family(
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "other_num"_id},
      product_query{
        .creator = "input"_id, .layer = "event"_id, .suffix = "num"_id}) // <= Note input order
    .experimental_when("odds_only");

  g.execute();

  CHECK(g.execution_count("check_odds") == 5);
  CHECK(g.execution_count("check_evens") == 5);
}
