// =======================================================================================
// This test executes unfolding functionality using the following graph
//
//     Index Router
//          |
//      unfold (creates children)
//          |
//         add(*)
//          |
//     print_result
//
// where the asterisk (*) indicates a fold step.  The difference here is that the
// *unfold* is responsible for sending the flush token instead of the
// source/index_router.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"
#include "test/products_for_output.hpp"

#include "catch2/catch_test_macros.hpp"

#include <atomic>
#include <string>

using namespace phlex;

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
    auto unfold(numbers_t::const_iterator it, data_cell_index const& lid) const
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
    if (sum.data_cell_index().number() == 0ull) {
      CHECK(*sum == 45);
    } else {
      CHECK(*sum == 190);
    }
  }

  void check_sum_same(handle<unsigned int> const sum)
  {
    auto const expected_sum = (sum.data_cell_index().number() + 1) * 10;
    CHECK(*sum == expected_sum);
  }

  // Provider algorithms
  unsigned int provide_max_number(data_cell_index const& id) { return 10u * (id.number() + 1); }

  numbers_t provide_ten_numbers(data_cell_index const& id)
  {
    return numbers_t(10, id.number() + 1);
  }
}

TEST_CASE("Splitting the processing", "[graph]")
{
  constexpr auto index_limit = 2u;

  experimental::layer_generator gen;
  gen.add_layer("event", {"job", index_limit});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("provide_max_number", provide_max_number, concurrency::unlimited)
    .output_product("max_number"_in("event"));
  g.provide("provide_ten_numbers", provide_ten_numbers, concurrency::unlimited)
    .output_product("ten_numbers"_in("event"));

  g.unfold<iota>("iota", &iota::predicate, &iota::unfold, concurrency::unlimited, "lower1")
    .input_family("max_number"_in("event"))
    .output_products("new_number");
  g.fold("add", add, concurrency::unlimited, "event")
    .input_family("new_number"_in("lower1"))
    .output_products("sum1");
  g.observe("check_sum", check_sum, concurrency::unlimited).input_family("sum1"_in("event"));

  g.unfold<iterate_through>("iterate_through",
                            &iterate_through::predicate,
                            &iterate_through::unfold,
                            concurrency::unlimited,
                            "lower2")
    .input_family("ten_numbers"_in("event"))
    .output_products("each_number");
  g.fold("add_numbers", add_numbers, concurrency::unlimited, "event")
    .input_family("each_number"_in("lower2"))
    .output_products("sum2");
  g.observe("check_sum_same", check_sum_same, concurrency::unlimited)
    .input_family("sum2"_in("event"));

  g.make<experimental::test::products_for_output>().output(
    "save", &experimental::test::products_for_output::save, concurrency::serial);

  g.execute();

  CHECK(g.execution_count("iota") == index_limit);
  CHECK(g.execution_count("add") == 30);
  CHECK(g.execution_count("check_sum") == index_limit);

  CHECK(g.execution_count("iterate_through") == index_limit);
  CHECK(g.execution_count("add_numbers") == 20);
  CHECK(g.execution_count("check_sum_same") == index_limit);
}
