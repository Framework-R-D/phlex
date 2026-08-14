#include "phlex/model/data_product_concept_registry.hpp"

#include "catch2/catch_test_macros.hpp"

#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

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

TEST_CASE("newly constructed registry has no concept names", "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  CHECK(registry.all_concept_names().empty());
}

TEST_CASE("register_concept registers a new concept", "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  auto con = std::make_unique<data_product_concept>("test_concept");
  CHECK(registry.register_concept(std::move(con)) == true);

  auto const* found = registry.find_concept("test_concept");
  CHECK(found != nullptr);
  CHECK(found->name() == "test_concept");
}

TEST_CASE("register_concept preserves the concrete types of a pre-populated concept",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  auto con = std::make_unique<data_product_concept>("populated_concept");
  con->add_concrete_type(typeid(test_type_1));
  con->add_concrete_type(typeid(test_type_2));

  CHECK(registry.register_concept(std::move(con)) == true);

  auto const* found = registry.find_concept("populated_concept");
  CHECK(found != nullptr);
  CHECK(found->has_concrete_type(typeid(test_type_1)));
  CHECK(found->has_concrete_type(typeid(test_type_2)));
}

TEST_CASE("register_concept(nullptr) throws std::invalid_argument",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  CHECK_THROWS_AS(registry.register_concept(nullptr), std::invalid_argument);
}

TEST_CASE("register_concept merges concrete types on duplicate name",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  auto con1 = std::make_unique<data_product_concept>("duplicate");
  con1->add_concrete_type(typeid(test_type_1));

  auto con2 = std::make_unique<data_product_concept>("duplicate");
  con2->add_concrete_type(typeid(test_type_1));
  con2->add_concrete_type(typeid(test_type_2));

  CHECK(registry.register_concept(std::move(con1)) == true);
  CHECK(registry.register_concept(std::move(con2)) == false);

  auto const names = registry.all_concept_names();
  CHECK(names.size() == 1ull);
  CHECK(names[0] == "duplicate");

  CHECK(registry.type_matches_concept("duplicate", typeid(test_type_1)));
  CHECK(registry.type_matches_concept("duplicate", typeid(test_type_2)));

  auto const* found = registry.find_concept("duplicate");
  CHECK(found != nullptr);
  CHECK(found->concrete_types().size() == 2ull);
}

TEST_CASE("add_concrete_type adds a type to an empty registry", "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  CHECK(registry.add_concrete_type("test_concept", typeid(test_type_1)) == true);

  auto const* found = registry.find_concept("test_concept");
  CHECK(found != nullptr);
  CHECK(found->has_concrete_type(typeid(test_type_1)));
}

TEST_CASE("add_concrete_type adds a type to an existing concept",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  CHECK(registry.add_concrete_type("test_concept", typeid(test_type_1)) == true);
  CHECK(registry.add_concrete_type("test_concept", typeid(test_type_2)) == false);

  auto const* found = registry.find_concept("test_concept");
  CHECK(found != nullptr);
  CHECK(found->has_concrete_type(typeid(test_type_1)));
  CHECK(found->has_concrete_type(typeid(test_type_2)));
}

TEST_CASE("add_concrete_type associates one concrete type with multiple concepts",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  registry.add_concrete_type("concept_a", typeid(test_type_1));
  registry.add_concrete_type("concept_b", typeid(test_type_1));

  CHECK(registry.type_matches_concept("concept_a", typeid(test_type_1)));
  CHECK(registry.type_matches_concept("concept_b", typeid(test_type_1)));
}

TEST_CASE("add_concrete_type is idempotent for a duplicate type",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  CHECK(registry.add_concrete_type("test_concept", typeid(test_type_1)) == true);
  CHECK(registry.add_concrete_type("test_concept", typeid(test_type_1)) == false);

  auto const* found = registry.find_concept("test_concept");
  CHECK(found != nullptr);
  CHECK(found->concrete_types().size() == 1ull);
}

TEST_CASE("add_concrete_type and register_concept produce consistent registry state",
          "[data_product_concept_registry]")
{
  data_product_concept_registry via_add_concrete_type;
  via_add_concrete_type.add_concrete_type("consistent_concept", typeid(test_type_1));
  via_add_concrete_type.add_concrete_type("consistent_concept", typeid(test_type_2));

  data_product_concept_registry via_register_concept;
  auto con = std::make_unique<data_product_concept>("consistent_concept");
  con->add_concrete_type(typeid(test_type_1));
  con->add_concrete_type(typeid(test_type_2));
  via_register_concept.register_concept(std::move(con));

  for (auto const* registry : {&via_add_concrete_type, &via_register_concept}) {
    auto const names = registry->all_concept_names();
    CHECK(names.size() == 1ull);
    CHECK(names[0] == "consistent_concept");

    CHECK(registry->type_matches_concept("consistent_concept", typeid(test_type_1)));
    CHECK(registry->type_matches_concept("consistent_concept", typeid(test_type_2)));
    CHECK_FALSE(registry->type_matches_concept("consistent_concept", typeid(int)));
  }
}

TEST_CASE("find_concept (non-const) finds a registered concept",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  registry.register_concept(std::make_unique<data_product_concept>("test_concept"));

  data_product_concept* found = registry.find_concept("test_concept");
  CHECK(found != nullptr);
  CHECK(found->name() == "test_concept");
}

TEST_CASE("find_concept (const) finds a registered concept", "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  registry.register_concept(std::make_unique<data_product_concept>("test_concept"));

  data_product_concept const* found = std::as_const(registry).find_concept("test_concept");
  CHECK(found != nullptr);
  CHECK(found->name() == "test_concept");
}

TEST_CASE("find_concept (non-const) returns nullptr for an unknown name",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  CHECK(registry.find_concept("nonexistent") == nullptr);
}

TEST_CASE("find_concept (const) returns nullptr for an unknown name",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  CHECK(std::as_const(registry).find_concept("nonexistent") == nullptr);
}

TEST_CASE("all_concept_names returns every name exactly once in sorted order",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  registry.register_concept(std::make_unique<data_product_concept>("concept_c"));
  registry.register_concept(std::make_unique<data_product_concept>("concept_a"));
  registry.register_concept(std::make_unique<data_product_concept>("concept_b"));

  auto const names = registry.all_concept_names();
  CHECK(names.size() == 3ull);
  CHECK(names[0] == "concept_a");
  CHECK(names[1] == "concept_b");
  CHECK(names[2] == "concept_c");
}

TEST_CASE("type_matches_concept is true for an associated type",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  auto con = std::make_unique<data_product_concept>("numeric");
  con->add_concrete_type(typeid(int));
  registry.register_concept(std::move(con));

  CHECK(registry.type_matches_concept("numeric", typeid(int)) == true);
}

TEST_CASE("type_matches_concept is false for an unassociated type",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  auto con = std::make_unique<data_product_concept>("numeric");
  con->add_concrete_type(typeid(int));
  registry.register_concept(std::move(con));

  CHECK(registry.type_matches_concept("numeric", typeid(double)) == false);
}

TEST_CASE("type_matches_concept is false for an unknown concept",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  CHECK(registry.type_matches_concept("nonexistent", typeid(int)) == false);
}

TEST_CASE("type_matches_concept is true under both names for a shared type",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  registry.add_concrete_type("concept_a", typeid(test_type_1));
  registry.add_concrete_type("concept_b", typeid(test_type_1));

  CHECK(registry.type_matches_concept("concept_a", typeid(test_type_1)));
  CHECK(registry.type_matches_concept("concept_b", typeid(test_type_1)));
}

TEST_CASE("get_concept_for_type returns the associated concept",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  auto con = std::make_unique<data_product_concept>("type_a");
  con->add_concrete_type(typeid(test_type_1));
  registry.register_concept(std::move(con));

  auto const* found = registry.get_concept_for_type(typeid(test_type_1));
  CHECK(found != nullptr);
  CHECK(found->name() == "type_a");
}

TEST_CASE("get_concept_for_type returns nullptr for an unknown type",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;
  CHECK(registry.get_concept_for_type(typeid(int)) == nullptr);
}

TEST_CASE("get_concept_for_type returns a non-null match when a type belongs to multiple "
          "concepts",
          "[data_product_concept_registry]")
{
  data_product_concept_registry registry;

  registry.add_concrete_type("concept_a", typeid(test_type_1));
  registry.add_concrete_type("concept_b", typeid(test_type_1));

  auto const* found = registry.get_concept_for_type(typeid(test_type_1));
  CHECK(found != nullptr);
  CHECK(found->has_concrete_type(typeid(test_type_1)));
}
