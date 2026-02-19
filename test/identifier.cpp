#include "phlex/model/identifier.hpp"
#include <phlex/configuration.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <string_view>

#include <boost/json.hpp>
#include <fmt/format.h>

int main()
{
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;
  using namespace std::string_view_literals;
  identifier a = "a"_id;
  identifier a_copy = a;
  identifier a2 = "a"_id;
  identifier b = "b"_id;

  boost::json::object parsed_json = boost::json::parse(R"( {"identifier": "b" } )").as_object();
  auto b_from_json = phlex::detail::value_if_exists(parsed_json, "identifier");
  assert(b_from_json);

  fmt::print("a ({}) == \"a\"_idq: {}\n", a, a == "a"_idq);
  fmt::print("a == a_copy ({}): {}\n", a_copy, a == a_copy);
  fmt::print("a == a2 ({}): {}\n", a2, a == a2);
  fmt::print("a != b ({}): {}\n", b, a != b);
  fmt::print("b == *b_from_json ({}): {}\n", *b_from_json, b == *b_from_json);

  assert(a == "a"_idq);
  assert(a == a_copy);
  assert(a == a2);
  assert(a != b);
  assert(b == *b_from_json);

  // reassigning
  a = "new a"_id;
  a_copy = a;

  std::array<std::string_view, 9> strings{
    "a"sv, "b"sv, "c"sv, "d"sv, "e"sv, "long-id-1"sv, "long-id-2"sv, "test"sv, "other_test"sv};
  std::array<identifier, 9> identifiers{"a"_id,
                                        "b"_id,
                                        "c"_id,
                                        "d"_id,
                                        "e"_id,
                                        "long-id-1"_id,
                                        "long-id-2"_id,
                                        "test"_id,
                                        "other_test"_id};
  std::ranges::sort(identifiers);
  std::ranges::sort(strings);
  fmt::print("Sorted identifiers (should not be in alphabetical order): \n");
  for (auto const& id : identifiers) {
    fmt::print("- {}\n", id);
  }

  bool ok = false;
  for (std::size_t i = 0; i < identifiers.size(); ++i) {
    if (strings.at(i) != std::string_view(identifiers.at(i))) {
      ok = true;
      break;
    }
  }
  assert(ok);

  // Additional coverage for identifier edge cases

  // Ordering is _not_ lexical
  identifier id1("abc");
  identifier id2("def");

  assert(id1 == id1);
  assert(id1 != id2);
  assert(id1 < id2);
  assert(id2 > id1);
  assert(id1 <= id1);
  assert(id1 >= id1);

  return 0;
}
