#include "phlex/utilities/string_literal.hpp"

#include <string_view>

using namespace phlex::experimental;
using namespace std::string_view_literals;

auto main() -> int
{
  constexpr string_literal s{"Check, please"};
  static_assert(std::string_view{s} == "Check, please"sv);
  static_assert(are_unique<"A", "B">);
  static_assert(not are_unique<"A", "B", "A">);
}
