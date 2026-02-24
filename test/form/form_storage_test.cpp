//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numeric>
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

TEST_CASE("Storage_Container sharing an Association", "[form]")
{
  int const technology = form::technology::ROOT_TTREE;
  std::vector<float> piData(10, 3.1415927);
  std::string indexData = "[EVENT=00000001;SEG=00000001]";

  form::test::write(technology, piData, indexData);

  auto [piResult, indexResult] = form::test::read<std::vector<float>, std::string>(technology);
  assert(piResult);
  assert(indexResult);

  float const originalSum = std::accumulate(piData.begin(), piData.end(), 0.f);
  float const readSum = std::accumulate(piResult->begin(), piResult->end(), 0.f);
  float const floatDiff = readSum - originalSum;

  SECTION("float container sum") { CHECK(fabs(floatDiff) < std::numeric_limits<float>::epsilon()); }

  SECTION("index") { CHECK(*indexResult == indexData); }
}

TEST_CASE("Storage_Container multiple containers in Association", "[form]")
{
  int const technology = form::technology::ROOT_TTREE;
  std::vector<float> piData(10, 3.1415927);
  std::vector<int> magicData(17);
  std::iota(magicData.begin(), magicData.end(), 42);
  std::string indexData = "[EVENT=00000001;SEG=00000001]";

  form::test::write(technology, piData, magicData, indexData);

  auto [piResult, magicResult, indexResult] =
    form::test::read<std::vector<float>, std::vector<int>, std::string>(technology);
  assert(piResult);
  assert(magicResult);
  assert(indexResult);

  SECTION("float container")
  {
    float const originalSum = std::accumulate(piData.begin(), piData.end(), 0.f);
    float const readSum = std::accumulate(piResult->begin(), piResult->end(), 0.f);
    float const floatDiff = readSum - originalSum;
    CHECK(fabs(floatDiff) < std::numeric_limits<float>::epsilon());
  }

  SECTION("int container")
  {
    int const originalMagic = std::accumulate(magicData.begin(), magicData.end(), 0);
    int const readMagic = std::accumulate(magicResult->begin(), magicResult->end(), 0);
    int const magicDiff = readMagic - originalMagic;
    CHECK(magicDiff == 0);
  }

  SECTION("index data") { CHECK(*indexResult == indexData); }
}

TEST_CASE("FORM Container setup error handling")
{
  int const technology = form::technology::ROOT_TTREE;
  auto file = createFile(technology, "testContainerErrorHandling.root", 'o');
  auto container = createContainer(technology, "test/testData");

  std::vector<float> testData;
  void const* ptrTestData = &testData;
  auto const& typeInfo = typeid(testData);

  SECTION("fill() before setParent()")
  {
    CHECK_THROWS_AS(container->setupWrite(typeInfo), std::runtime_error);
    CHECK_THROWS_AS(container->fill(ptrTestData), std::runtime_error);
  }

  SECTION("commit() before setParent()") { CHECK_THROWS_AS(container->commit(), std::runtime_error); }

  SECTION("read() before setParent()") { CHECK_THROWS_AS(container->read(0, &ptrTestData, typeInfo), std::runtime_error); }

  SECTION("mismatched file type")
  {
    std::shared_ptr<IStorage_File> wrongFile(
    new Storage_File("testContainerErrorHandling.root", 'o'));
    CHECK_THROWS_AS(container->setFile(wrongFile), std::runtime_error);
  }
}
