#include "phlex/utilities/sized_tuple.hpp"

#include <concepts>
#include <tuple>

using namespace std;
using namespace phlex::experimental;

auto main() -> int
{
  static_assert(same_as<sized_tuple<int, 3>, tuple<int, int, int>>);
  static_assert(
    same_as<concatenated_tuples<tuple<int>, sized_tuple<double, 4>, tuple<float, float>>,
            tuple<int, double, double, double, double, float, float>>);
}
