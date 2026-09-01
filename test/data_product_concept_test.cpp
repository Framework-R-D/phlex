#include "phlex/model/data_product_concept.hpp"

#include "catch2/catch_test_macros.hpp"

#include <any>
#include <functional>
#include <stdexcept>
#include <string>
#include <typeinfo>

using namespace phlex::experimental;

namespace {
  struct soa_vertices {
    int value;
    auto operator<=>(soa_vertices const&) const = default;
  };

  struct aos_vertices {
    std::string value;
    auto operator<=>(aos_vertices const&) const = default;
  };

  using conversion_to_soa = std::function<soa_vertices(aos_vertices const&)>;

  soa_vertices to_soa(aos_vertices const& in) { return soa_vertices{std::stoi(in.value)}; }

  // A concept modeled by both test types, ready for translator registration.
  data_product_concept concept_with_both_types()
  {
    data_product_concept con{"VertexCollection"};
    con.add_concrete_type(typeid(soa_vertices));
    con.add_concrete_type(typeid(aos_vertices));
    return con;
  }
}

TEST_CASE("data_product_concept construction records the right name", "[data_product_concept]")
{
  data_product_concept con{"my_concept"};
  CHECK(con.name() == "my_concept");
}

TEST_CASE("data_product_concept starts with no concrete types", "[data_product_concept]")
{
  data_product_concept con{"empty_concept"};
  CHECK(con.concrete_types().empty());
}

TEST_CASE("data_product_concept add and check concrete types", "[data_product_concept]")
{
  data_product_concept con{"VertexCollection"};

  auto const& type1 = typeid(soa_vertices);
  auto const& type2 = typeid(aos_vertices);

  con.add_concrete_type(type1);
  con.add_concrete_type(type2);

  CHECK(con.concrete_types().size() == 2ull);
  CHECK(con.has_concrete_type(type1));
  CHECK(con.has_concrete_type(type2));
  CHECK_FALSE(con.has_concrete_type(typeid(int)));
}

TEST_CASE("data_product_concept equality", "[data_product_concept]")
{
  data_product_concept concept1{"same_name"};
  data_product_concept concept2{"same_name"};
  data_product_concept concept3{"different_name"};

  CHECK(concept1 == concept2);
  CHECK(concept1 != concept3);
}

TEST_CASE("data_product_concept with multiple primitive types", "[data_product_concept]")
{
  data_product_concept con{"shared_concept"};

  auto const& type1 = typeid(int);
  auto const& type2 = typeid(long);

  con.add_concrete_type(type1);
  con.add_concrete_type(type2);

  CHECK(con.has_concrete_type(type1));
  CHECK(con.has_concrete_type(type2));
}

TEST_CASE("adding a concrete type is idempotent", "[data_product_concept]")
{
  data_product_concept con{"VertexCollection"};

  auto const& type1 = typeid(soa_vertices);

  con.add_concrete_type(type1);
  con.add_concrete_type(type1); // Add again

  CHECK(con.concrete_types().size() == 1ull);
  CHECK(con.has_concrete_type(type1));
}

TEST_CASE("data_product_concept construction with an empty name throws", "[data_product_concept]")
{
  CHECK_THROWS_AS(data_product_concept{""}, std::invalid_argument);
}

TEST_CASE("add_concrete_types adds the union of an overlapping set", "[data_product_concept]")
{
  data_product_concept con{"VertexCollection"};
  con.add_concrete_type(typeid(soa_vertices));

  con.add_concrete_types({typeid(soa_vertices), typeid(aos_vertices)});

  CHECK(con.concrete_types().size() == 2ull);
  CHECK(con.has_concrete_type(typeid(soa_vertices)));
  CHECK(con.has_concrete_type(typeid(aos_vertices)));
}

TEST_CASE("a concept starts with no registered translators", "[data_product_concept]")
{
  auto con = concept_with_both_types();
  CHECK(con.find_translator(typeid(soa_vertices), typeid(aos_vertices)) == nullptr);
}

TEST_CASE("a registered translator round-trips through the accessor", "[data_product_concept]")
{
  auto con = concept_with_both_types();

  con.add_translator("one_to_two",
                     typeid(soa_vertices),
                     typeid(aos_vertices),
                     conversion_to_soa{to_soa},
                     result_storage::owned);

  auto const* registration = con.find_translator(typeid(soa_vertices), typeid(aos_vertices));
  REQUIRE(registration != nullptr);
  CHECK(registration->name == "one_to_two");
  CHECK(registration->storage == result_storage::owned);

  auto const* conversion = std::any_cast<conversion_to_soa>(&registration->function);
  REQUIRE(conversion != nullptr);
  aos_vertices input{"7"};
  soa_vertices const result = (*conversion)(input);
  CHECK(result == soa_vertices{7});
}

TEST_CASE("a borrowed result storage declaration is preserved", "[data_product_concept]")
{
  auto con = concept_with_both_types();

  con.add_translator("one_to_two",
                     typeid(soa_vertices),
                     typeid(aos_vertices),
                     conversion_to_soa{to_soa},
                     result_storage::borrowed);

  auto const* registration = con.find_translator(typeid(soa_vertices), typeid(aos_vertices));
  REQUIRE(registration != nullptr);
  CHECK(registration->storage == result_storage::borrowed);
}

TEST_CASE("translators are keyed by the ordered type pair", "[data_product_concept]")
{
  auto con = concept_with_both_types();

  con.add_translator("one_to_two",
                     typeid(soa_vertices),
                     typeid(aos_vertices),
                     conversion_to_soa{to_soa},
                     result_storage::owned);

  // The reverse conversion is a different translator, and is not registered.
  CHECK(con.find_translator(typeid(aos_vertices), typeid(soa_vertices)) == nullptr);
}

TEST_CASE("registering a translator with identical source and target types throws",
          "[data_product_concept]")
{
  auto con = concept_with_both_types();

  CHECK_THROWS_AS(con.add_translator("one_to_one",
                                     typeid(soa_vertices),
                                     typeid(soa_vertices),
                                     std::function<soa_vertices(aos_vertices const&)>{},
                                     result_storage::owned),
                  std::invalid_argument);
}

TEST_CASE("registering a translator for a type outside the concept throws",
          "[data_product_concept]")
{
  data_product_concept con{"VertexCollection"};
  con.add_concrete_type(typeid(soa_vertices));

  // The target type does not model the concept.
  CHECK_THROWS_AS(con.add_translator("one_to_two",
                                     typeid(soa_vertices),
                                     typeid(aos_vertices),
                                     conversion_to_soa{to_soa},
                                     result_storage::owned),
                  std::invalid_argument);

  // Nor does the source type, once the roles are reversed.
  CHECK_THROWS_AS(con.add_translator("two_to_one",
                                     typeid(aos_vertices),
                                     typeid(soa_vertices),
                                     std::function<soa_vertices(aos_vertices const&)>{},
                                     result_storage::owned),
                  std::invalid_argument);

  CHECK(con.find_translator(typeid(soa_vertices), typeid(aos_vertices)) == nullptr);
}

TEST_CASE("registering a second translator for the same type pair throws", "[data_product_concept]")
{
  auto con = concept_with_both_types();

  con.add_translator("one_to_two",
                     typeid(soa_vertices),
                     typeid(aos_vertices),
                     conversion_to_soa{to_soa},
                     result_storage::owned);

  CHECK_THROWS_AS(con.add_translator("another_one_to_two",
                                     typeid(soa_vertices),
                                     typeid(aos_vertices),
                                     conversion_to_soa{to_soa},
                                     result_storage::owned),
                  std::invalid_argument);

  // The original registration is intact.
  auto const* registration = con.find_translator(typeid(soa_vertices), typeid(aos_vertices));
  REQUIRE(registration != nullptr);
  CHECK(registration->name == "one_to_two");
}
