#include "phlex/model/identifier.hpp"
#include <phlex/configuration.hpp>

#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <array>
#include <string_view>

#include <boost/json.hpp>
#include <fmt/format.h>

using namespace phlex::experimental;
using namespace phlex::experimental::literals;
using namespace std::string_view_literals;

TEST_CASE("Identifier equality and inequality", "[identifier]")
{
  identifier a = "a"_id;
  identifier a_copy = a;
  identifier a2 = "a"_id;
  identifier b = "b"_id;

  CHECK(a == "a"_idq);
  CHECK(a == a_copy);
  CHECK(a == a2);
  CHECK(a != b);
}

TEST_CASE("Identifier from JSON", "[identifier]")
{
  identifier b = "b"_id;
  boost::json::object parsed_json = boost::json::parse(R"( {"identifier": "b" } )").as_object();
  auto b_from_json = phlex::detail::value_if_exists(parsed_json, "identifier");
  REQUIRE(b_from_json);
  CHECK(b == *b_from_json);
}

TEST_CASE("Identifier reassignment", "[identifier]")
{
  identifier a = "a"_id;
  identifier a_copy = a;
  a = "new a"_id;
  a_copy = a;
  CHECK(a == a_copy);
}

TEST_CASE("Identifier sorting is not lexical", "[identifier]")
{
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

  bool differs = false;
  for (std::size_t i = 0; i < identifiers.size(); ++i) {
    if (strings.at(i) != std::string_view(identifiers.at(i))) {
      differs = true;
      break;
    }
  }
  CHECK(differs);
}

TEST_CASE("Identifier comparison operators", "[identifier]")
{
  identifier id1("abc");
  identifier id2("def");

  CHECK(id1 == id1);
  CHECK(id1 != id2);
  CHECK(id1 < id2);
  CHECK(id2 > id1);
  CHECK(id1 <= id1);
  CHECK(id1 >= id1);
}

TEST_CASE("Identifier formatting", "[identifier]")
{
  identifier id = "test_id"_id;
  auto formatted = fmt::format("{}", format_as(id));
  CHECK(formatted == "test_id");
  CHECK(fmt::format("prefix_{}_suffix", id) == "prefix_test_id_suffix");
}
