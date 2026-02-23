//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace form::detail::experimental;

TEST_CASE("Storage_Container read wrong type", "[form]")
{
  int const technology = form::technology::ROOT_TTREE;
  std::vector<int> primes = {2, 3, 5, 7, 11, 13, 17, 19};
  form::test::write(technology, primes);

  auto file = createFile(technology, form::test::testFileName, 'i');
  auto container = createContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
  container->setFile(file);
  void const* dataPtr;
  CHECK_THROWS_AS(container->read(0, &dataPtr, typeid(double)), std::runtime_error);
}
