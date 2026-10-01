#include "core/technology.hpp"
#include "storage/factories.hpp"
#include "storage/storage_associative_write_container.hpp"
#include "test/form/data_products/unserializable.hpp"
#include "test/form/test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

auto constexpr tech = form::technology::root_rntuple;

TEST_CASE("automatic streamer mode", "[form RNTuple]")
{
  unserializable empty(1);
  std::stringstream const std_err_redirect;
  auto* original_cerr = std::cerr.rdbuf(std_err_redirect.rdbuf());

  form::test::write(tech, empty);

  std::cerr.rdbuf(original_cerr);

  REQUIRE(!std_err_redirect.str().empty());
}

TEST_CASE("RNTuple open failure", "[form RNTuple]")
{
  //TODO: create a read-only file and pass it to RNTuple write backend
  double test_data = 42.;
  auto file = create_file(tech, "form_read_only_file_rntuple.root", 'z');
  auto assoc = create_write_association(tech, "test");
  assoc->set_file(file);
  assoc->setup_write(typeid(test_data));
  auto writer = create_write_container(tech, "test/test_data");
  dynamic_pointer_cast<storage_associative_write_container>(writer)->set_parent(assoc);
  writer->setup_write(typeid(test_data));
  CHECK_THROWS_AS(writer->fill(&test_data), std::runtime_error);
}
