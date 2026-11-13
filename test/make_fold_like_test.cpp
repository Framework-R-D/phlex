#include "phlex/metaprogramming/make_fold_like.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"

#include <catch2/catch_test_macros.hpp>
#include <gsl/pointers>

#include <functional>
#include <tuple>
#include <type_traits>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::experimental::literals;

namespace {
  /// Value type that counts how often it is copied or moved.
  /// Call reset() before the operation whose copies and moves are to be counted.
  struct tracked {
    static inline int copies = 0;
    static inline int moves = 0;
    int value = 0;

    /// Construct with the given value.
    explicit tracked(int v = 0) : value{v} {}
    /// Copy, incrementing the copy counter.
    tracked(tracked const& other) : value{other.value} { ++copies; }
    /// Move, incrementing the move counter.
    tracked(tracked&& other) noexcept : value{other.value} { ++moves; }
    tracked& operator=(tracked const&) = delete;
    tracked& operator=(tracked&&) = delete;
    ~tracked() = default;
    /// Reset both counters to zero.
    static void reset() { copies = moves = 0; }
  };

  /// Fold with two accumulators and a by-value input.
  // NOLINTNEXTLINE(performance-unnecessary-value-param): the by-value input is what is tested
  void fold_by_value(tracked& sum, int& count, tracked in)
  {
    sum.value += in.value;
    ++count;
  }

  /// Fold with two accumulators and a const lvalue reference input.
  void fold_by_cref(tracked& sum, int& count, tracked const& in)
  {
    sum.value += in.value;
    ++count;
  }

  /// Fold with two accumulators and a pointer-to-const input.
  void fold_by_cptr(tracked& sum, int& count, tracked const* in)
  {
    sum.value += in->value;
    ++count;
  }

  /// Fold with two accumulators and a handle input.
  void fold_by_handle(tracked& sum, int& count, handle<tracked> in)
  {
    sum.value += in->value;
    ++count;
  }
}

TEST_CASE("Leading non-const lvalue references are the accumulators", "[make_fold_like]")
{
  using traits = fold_like_traits<decltype(&fold_by_cref)>;
  STATIC_REQUIRE(traits::num_accumulators::value == 2);
  STATIC_REQUIRE(std::is_same_v<traits::accumulator_tuple_t, std::tuple<tracked, int>>);
  STATIC_REQUIRE(std::is_same_v<traits::input_args_t, std::tuple<tracked const&>>);
}

TEST_CASE("A single-accumulator fold is returned unchanged", "[make_fold_like]")
{
  auto single = [](int& sum, int in) { sum += in; };
  STATIC_REQUIRE(
    std::is_same_v<std::remove_cvref_t<decltype(make_fold_like(single))>, decltype(single)>);
}

TEST_CASE("Accumulators are updated in place without copies", "[make_fold_like]")
{
  auto fold = make_fold_like(&fold_by_cref);
  std::tuple<tracked, int> acc{};
  tracked const in{3};

  tracked::reset();
  fold(acc, in);
  fold(acc, in);

  CHECK(std::get<0>(acc).value == 6);
  CHECK(std::get<1>(acc) == 2);
  CHECK(tracked::copies == 0);
  CHECK(tracked::moves == 0);
}

TEST_CASE("Rebinding a reference_wrapper accumulator persists", "[make_fold_like]")
{
  int other_target = 0;
  // Add the input through the wrapper, then rebind the wrapper to other_target.
  auto rebind = [&other_target](std::reference_wrapper<int>& target, int& count, int in) {
    target.get() += in;
    target = std::ref(other_target);
    ++count;
  };
  auto fold = make_fold_like(rebind);
  int original = 1;
  std::tuple<std::reference_wrapper<int>, int> acc{std::ref(original), 0};

  fold(acc, 5);

  CHECK(original == 6);
  CHECK(&std::get<0>(acc).get() == &other_target);
  CHECK(std::get<1>(acc) == 1);
}

TEST_CASE("By-value inputs are copied or moved only into the adapter's parameter",
          "[make_fold_like]")
{
  auto fold = make_fold_like(&fold_by_value);
  std::tuple<tracked, int> acc{};

  SECTION("lvalue argument")
  {
    tracked const in{3};
    tracked::reset();
    fold(acc, in);
    // One copy into the adapter's parameter, then one move into the fold's parameter.
    CHECK(tracked::copies == 1);
    CHECK(tracked::moves == 1);
  }

  SECTION("prvalue argument")
  {
    tracked::reset();
    fold(acc, tracked{3});
    // The adapter's parameter is initialized directly, then moved into the fold's parameter.
    CHECK(tracked::copies == 0);
    CHECK(tracked::moves == 1);
  }

  CHECK(std::get<0>(acc).value == 3);
}

TEST_CASE("Const lvalue reference inputs are neither copied nor moved", "[make_fold_like]")
{
  auto fold = make_fold_like(&fold_by_cref);
  std::tuple<tracked, int> acc{};
  tracked const in{3};

  tracked::reset();
  fold(acc, in);

  CHECK(tracked::copies == 0);
  CHECK(tracked::moves == 0);
  CHECK(std::get<0>(acc).value == 3);
}

TEST_CASE("Pointer-to-const inputs point at the caller's object", "[make_fold_like]")
{
  auto fold = make_fold_like(&fold_by_cptr);
  std::tuple<tracked, int> acc{};
  tracked const in{3};

  tracked::reset();
  fold(acc, &in);

  CHECK(tracked::copies == 0);
  CHECK(tracked::moves == 0);
  CHECK(std::get<0>(acc).value == 3);
}

TEST_CASE("Handle inputs do not copy the product", "[make_fold_like]")
{
  auto fold = make_fold_like(&fold_by_handle);
  std::tuple<tracked, int> acc{};
  tracked const product{3};
  auto const index = data_cell_index::job();
  experimental::product_specification const spec{"product"};
  auto const stage = "test_stage"_id;
  handle const in{gsl::make_not_null(&product),
                  gsl::make_not_null(index.get()),
                  gsl::make_not_null(&spec),
                  gsl::make_not_null(&stage)};

  tracked::reset();
  fold(acc, in);

  CHECK(tracked::copies == 0);
  CHECK(tracked::moves == 0);
  CHECK(std::get<0>(acc).value == 3);
}
