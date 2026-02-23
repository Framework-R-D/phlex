//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <numeric>
#include <cmath>

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

TEST_CASE("Storage_Container sharing an Association", "[form]")
{
  int const technology = form::technology::ROOT_TTREE;
  std::vector<float> piData(10, 3.1415927);
  std::string indexData = "[EVENT=00000001;SEG=00000001]";

  form::test::write(technology, piData, indexData);

  auto [piResult, indexResult] = form::test::read<std::vector<float>, std::string>(technology);

  float const originalSum = std::accumulate(piData.begin(), piData.end(), 0.f);
  float const readSum = std::accumulate(piResult.begin(), piResult.end(), 0.f);
  float const floatDiff = readSum - originalSum;

  SECTION("float container sum")
  {
    CHECK(fabs(floatDiff) < std::numeric_limits<float>::epsilon());
  }

  SECTION("index")
  {
    CHECK(indexResult == indexData);
  }
}
