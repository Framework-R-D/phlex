#include "phlex/model/data_product_concept.hpp"

#include "catch2/catch_test_macros.hpp"

#include <stdexcept>
#include <string>
#include <typeinfo>

using namespace phlex::experimental;

namespace {
  struct test_type_1 {
    int value;
    auto operator<=>(test_type_1 const&) const = default;
  };

  struct test_type_2 {
    std::string value;
    auto operator<=>(test_type_2 const&) const = default;
  };
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
  data_product_concept con{"test_concept"};

  auto const& type1 = typeid(test_type_1);
  auto const& type2 = typeid(test_type_2);

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
  data_product_concept con{"concept_A"};

  auto const& type1 = typeid(test_type_1);

  con.add_concrete_type(type1);
  con.add_concrete_type(type1); // Add again

  CHECK(con.concrete_types().size() == 1ull);
  CHECK(con.has_concrete_type(type1));
}

TEST_CASE("data_product_concept construction with an empty name throws",
          "[data_product_concept]")
{
  CHECK_THROWS_AS(data_product_concept{""}, std::invalid_argument);
}

TEST_CASE("add_concrete_types adds the union of an overlapping set", "[data_product_concept]")
{
  data_product_concept con{"concept_A"};
  con.add_concrete_type(typeid(test_type_1));

  con.add_concrete_types({typeid(test_type_1), typeid(test_type_2)});

  CHECK(con.concrete_types().size() == 2ull);
  CHECK(con.has_concrete_type(typeid(test_type_1)));
  CHECK(con.has_concrete_type(typeid(test_type_2)));
}
