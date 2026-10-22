#include "phlex/configuration.hpp"
#include "phlex/model/identifier.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>

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
  boost::json::object const parsed_json =
    boost::json::parse(R"( {"identifier": "b" } )").as_object();
  auto b_from_json = phlex::detail::value_if_exists(parsed_json, "identifier");
  REQUIRE(b_from_json);
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- `REQUIRE` protects against incorrect access
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

TEST_CASE("empty_string formats as empty text", "[identifier]")
{
  CHECK(format_as(empty_string{}).empty());
  CHECK(fmt::format("[{}]", format_as(empty_string{})) == "[]");
}

TEST_CASE("empty_string values are all equal", "[identifier]")
{
  CHECK(empty_string{} == empty_string{});
  CHECK((empty_string{} <=> empty_string{}) == std::strong_ordering::equal);
}

TEST_CASE("Default-constructed identifier_or_empty is empty", "[identifier]")
{
  identifier_or_empty const empty{};

  CHECK(empty.empty());
  CHECK(empty.named() == nullptr);
  CHECK(empty.text().empty());
}

TEST_CASE("identifier_or_empty constructed from empty_string is empty", "[identifier]")
{
  identifier_or_empty const explicit_empty{empty_string{}};

  CHECK(explicit_empty.empty());
  CHECK(explicit_empty.named() == nullptr);
  CHECK(explicit_empty.text().empty());
}

TEST_CASE("identifier_or_empty constructed from an identifier is named", "[identifier]")
{
  identifier const run{"run_2"};
  identifier_or_empty const from_lvalue{run};
  identifier_or_empty const from_rvalue{"hits"_id};

  CHECK_FALSE(from_lvalue.empty());
  CHECK_FALSE(from_rvalue.empty());
  REQUIRE(from_lvalue.named() != nullptr);
  REQUIRE(from_rvalue.named() != nullptr);
  CHECK(*from_lvalue.named() == run);
  CHECK(*from_rvalue.named() == "hits"_id);
}

TEST_CASE("identifier_or_empty text() returns the held text", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const named{"hits"_id};

  CHECK(empty.text() == ""sv);
  CHECK(named.text() == "hits"sv);
}

TEST_CASE("identifier_or_empty formatting", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const named{"hits"_id};

  CHECK(fmt::format("[{}]", empty) == "[]");
  CHECK(fmt::format("{}", named) == "hits");
  CHECK(fmt::format("prefix_{}_suffix", named) == "prefix_hits_suffix");
  CHECK(format_as(empty).empty());
  CHECK(format_as(named) == "hits"sv);
}

TEST_CASE("identifier_or_empty empty values are equal to each other", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const explicit_empty{empty_string{}};

  CHECK(empty == explicit_empty);
  CHECK(explicit_empty == empty);
  CHECK_FALSE(empty != explicit_empty);
}

TEST_CASE("identifier_or_empty named values compare by identifier", "[identifier]")
{
  identifier_or_empty const hits{"hits"_id};
  identifier_or_empty const hits_copy{"hits"_id};
  identifier_or_empty const other{"other"_id};

  CHECK(hits == hits_copy);
  CHECK_FALSE(hits != hits_copy);
  CHECK(hits != other);
  CHECK(other != hits);
}

TEST_CASE("identifier_or_empty equality distinguishes empty from named", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const hits{"hits"_id};
  // A named value holding an identifier with empty text is still not equal to an empty value.
  identifier_or_empty const named_with_empty_text{identifier{}};

  CHECK(empty != hits);
  CHECK(hits != empty);
  CHECK(empty != named_with_empty_text);
  CHECK(named_with_empty_text != empty);
}

TEST_CASE("identifier_or_empty hash of empty values is shared", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const explicit_empty{empty_string{}};

  CHECK(empty.hash() == explicit_empty.hash());
  CHECK(std::hash<identifier_or_empty>{}(empty) ==
        std::hash<identifier_or_empty>{}(explicit_empty));
}

TEST_CASE("identifier_or_empty hash of named values is the identifier hash", "[identifier]")
{
  identifier const hits_id = "hits"_id;
  identifier_or_empty const hits{hits_id};
  identifier_or_empty const hits_copy{"hits"_id};

  CHECK(hits.hash() == hits_id.hash());
  CHECK(hits.hash() == hits_copy.hash());
}

TEST_CASE("identifier_or_empty hash accessors agree", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const hits{"hits"_id};

  CHECK(hash_value(empty) == empty.hash());
  CHECK(hash_value(hits) == hits.hash());
  CHECK(std::hash<identifier_or_empty>{}(empty) == empty.hash());
  CHECK(std::hash<identifier_or_empty>{}(hits) == hits.hash());
}

TEST_CASE("identifier_or_empty works as an unordered_set key", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const hits{"hits"_id};

  std::unordered_set<identifier_or_empty> const set{empty, hits};

  CHECK(set.size() == 2);
  CHECK(set.contains(identifier_or_empty{empty_string{}}));
  CHECK(set.contains(identifier_or_empty{"hits"_id}));
  CHECK_FALSE(set.contains(identifier_or_empty{"other"_id}));
}

TEST_CASE("identifier_or_empty orders empty before named", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const alpha{"alpha"_id};
  identifier_or_empty const beta{"beta"_id};

  CHECK(empty < alpha);
  CHECK(empty < beta);
  CHECK_FALSE(alpha < empty);
  CHECK_FALSE(beta < empty);
  CHECK((empty <=> alpha) == std::strong_ordering::less);
  CHECK((alpha <=> empty) == std::strong_ordering::greater);
}

TEST_CASE("identifier_or_empty empty values are ordered equivalently", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const explicit_empty{empty_string{}};

  CHECK((empty <=> explicit_empty) == std::strong_ordering::equal);
  CHECK_FALSE(empty < explicit_empty);
  CHECK_FALSE(explicit_empty < empty);
}

TEST_CASE("identifier_or_empty orders named values like identifiers", "[identifier]")
{
  identifier const alpha_id = "alpha"_id;
  identifier const beta_id = "beta"_id;
  identifier_or_empty const alpha{alpha_id};
  identifier_or_empty const beta{beta_id};

  CHECK((alpha <=> beta) == (alpha_id <=> beta_id));
  CHECK((beta <=> alpha) == (beta_id <=> alpha_id));
  CHECK((alpha <=> alpha) == std::strong_ordering::equal);
}

TEST_CASE("identifier_or_empty works as an ordered set key", "[identifier]")
{
  identifier_or_empty const empty{};
  identifier_or_empty const alpha{"alpha"_id};
  identifier_or_empty const beta{"beta"_id};

  std::set<identifier_or_empty> const ordered{beta, empty, alpha, empty};

  REQUIRE(ordered.size() == 3);
  CHECK(ordered.begin()->empty());
}

TEST_CASE("suffix_t is an alias of identifier_or_empty", "[identifier]")
{
  STATIC_CHECK(std::is_same_v<suffix_t, identifier_or_empty>);

  suffix_t const empty{empty_string{}};
  suffix_t const hits{"hits"_id};
  identifier_or_empty const empty_alias{};
  identifier_or_empty const hits_alias{"hits"_id};

  CHECK(empty == empty_alias);
  CHECK(hits == hits_alias);
  CHECK(empty.empty());
  CHECK(fmt::format("[{}]", empty) == "[]");
  CHECK(fmt::format("{}", hits) == "hits");
}

TEMPLATE_TEST_CASE("Empty-capable values implicitly accept C strings",
                   "[identifier]",
                   identifier_or_empty,
                   suffix_t)
{
  STATIC_CHECK(std::is_convertible_v<char const*, TestType>);

  TestType const empty = "";
  TestType const hits = "hits";
  TestType const terminated_hits = "hits\0ignored";
  TestType const expected_empty{empty_string{}};
  TestType const expected_hits{"hits"_id};

  CHECK(empty.empty());
  CHECK(empty.named() == nullptr);
  CHECK(empty.text() == ""sv);
  CHECK(empty == expected_empty);
  CHECK(empty.hash() == expected_empty.hash());
  CHECK(fmt::format("[{}]", empty) == "[]");
  CHECK_FALSE(hits.empty());
  REQUIRE(hits.named() != nullptr);
  CHECK(*hits.named() == "hits"_id);
  CHECK(hits.text() == "hits"sv);
  CHECK(hits == expected_hits);
  CHECK(hits.hash() == expected_hits.hash());
  CHECK(terminated_hits == hits);
  CHECK(terminated_hits.text() == "hits"sv);
  CHECK(fmt::format("{}", hits) == "hits");
  CHECK(empty < hits);
  CHECK(empty != hits);
}

TEMPLATE_TEST_CASE("Empty-capable values explicitly accept string objects",
                   "[identifier]",
                   identifier_or_empty,
                   suffix_t)
{
  STATIC_CHECK(std::is_constructible_v<TestType, std::string&>);
  STATIC_CHECK(std::is_constructible_v<TestType, std::string const&>);
  STATIC_CHECK(std::is_constructible_v<TestType, std::string>);
  STATIC_CHECK(std::is_constructible_v<TestType, std::string_view>);
  STATIC_CHECK_FALSE(std::is_convertible_v<std::string&, TestType>);
  STATIC_CHECK_FALSE(std::is_convertible_v<std::string const&, TestType>);
  STATIC_CHECK_FALSE(std::is_convertible_v<std::string, TestType>);
  STATIC_CHECK_FALSE(std::is_convertible_v<std::string_view, TestType>);
  STATIC_CHECK(std::is_convertible_v<empty_string, TestType>);
  STATIC_CHECK(std::is_convertible_v<identifier const&, TestType>);
  STATIC_CHECK(std::is_convertible_v<identifier, TestType>);
  STATIC_CHECK(std::is_nothrow_default_constructible_v<TestType>);

  std::string empty_text;
  std::string const hits_text{"hits"};
  std::array<TestType, 4> const empty_values{
    TestType{empty_text}, TestType{std::string{}}, TestType{std::string_view{}}, TestType{""sv}};
  std::array<TestType, 4> const named_values{TestType{hits_text},
                                             TestType{std::string{"hits"}},
                                             TestType{std::string_view{hits_text}},
                                             TestType{"hits"sv}};
  TestType const expected_empty{empty_string{}};
  TestType const expected_hits{"hits"_id};

  for (auto const& empty : empty_values) {
    CHECK(empty.empty());
    CHECK(empty.named() == nullptr);
    CHECK(empty.text() == ""sv);
    CHECK(empty == expected_empty);
    CHECK((empty <=> expected_empty) == std::strong_ordering::equal);
    CHECK(std::hash<TestType>{}(empty) == std::hash<TestType>{}(expected_empty));
    CHECK(hash_value(empty) == expected_empty.hash());
    CHECK(fmt::format("[{}]", empty) == "[]");
  }
  for (auto const& named : named_values) {
    CHECK_FALSE(named.empty());
    REQUIRE(named.named() != nullptr);
    CHECK(*named.named() == "hits"_id);
    CHECK(named.text() == "hits"sv);
    CHECK(named == expected_hits);
    CHECK((named <=> expected_hits) == std::strong_ordering::equal);
    CHECK(std::hash<TestType>{}(named) == std::hash<TestType>{}(expected_hits));
    CHECK(hash_value(named) == expected_hits.hash());
    CHECK(fmt::format("{}", named) == "hits");
    CHECK(expected_empty < named);
  }
}

TEMPLATE_TEST_CASE("Empty-capable values own exactly the supplied text range",
                   "[identifier]",
                   identifier_or_empty,
                   suffix_t)
{
  std::string backing_text{"hits_extra"};
  TestType const from_view{std::string_view{backing_text}.substr(0, 4)};
  TestType const from_string{backing_text};
  TestType const from_c_string = backing_text.c_str();
  backing_text.assign("other");

  CHECK(from_view.text() == "hits"sv);
  CHECK(from_view == TestType{"hits"_id});
  CHECK(from_view.hash() == TestType{"hits"_id}.hash());
  CHECK(from_string.text() == "hits_extra"sv);
  CHECK(from_c_string.text() == "hits_extra"sv);
  CHECK(from_string == from_c_string);
  CHECK(from_string.hash() == from_c_string.hash());
}
