//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include "TFile.h"
#include "TTree.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numeric>
#include <vector>

using namespace form::detail::experimental;

namespace {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  int technology = form::technology::ROOT_TTREE; //Potentially overridden in main
  //Non-const global variable required by limitations of Catch2
}

int main(int const argc, char** const argv)
{
  Catch::Session session;

  std::string tech_string;
  using namespace Catch::Clara;
  auto cli =
    session.cli() | Opt(tech_string, "technology")["--technology"]("FORM technology backend");

  session.cli(cli);

  int const returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0)
    return returnCode;

  technology = form::test::getTechnology(tech_string);

  return session.run();
}

TEST_CASE("Storage_Container read wrong type", "[form]")
{
  std::vector<int> primes = {2, 3, 5, 7, 11, 13, 17, 19};
  form::test::write(technology, primes);

  auto file = createFile(technology, form::test::testFileName, 'i');
  auto container =
    createReadContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
  container->setFile(file);
  void const* dataPtr = nullptr;
  CHECK_THROWS_AS(container->read(0, &dataPtr, typeid(double)), std::runtime_error);
}

TEST_CASE("Storage_Container sharing an Association", "[form]")
{
  std::vector<float> piData(10, 3.1415927);
  std::string indexData = "[EVENT=00000001;SEG=00000001]";

  form::test::write(technology, piData, indexData);

  auto [piResult, indexResult] = form::test::read<std::vector<float>, std::string>(technology);

  SECTION("float container") { CHECK(*piResult == piData); }

  SECTION("index") { CHECK(*indexResult == indexData); }
}

TEST_CASE("Storage_Container multiple containers in Association", "[form]")
{
  std::vector<float> piData(10, 3.1415927);
  std::vector<int> magicData(17);
  std::iota(magicData.begin(), magicData.end(), 42);
  std::string indexData = "[EVENT=00000001;SEG=00000001]";

  form::test::write(technology, piData, magicData, indexData);

  auto [piResult, magicResult, indexResult] =
    form::test::read<std::vector<float>, std::vector<int>, std::string>(technology);

  SECTION("float container") { CHECK(*piResult == piData); }

  SECTION("int container") { CHECK(*magicResult == magicData); }

  SECTION("index data") { CHECK(*indexResult == indexData); }
}

TEST_CASE("FORM Container setup error handling")
{
  auto file = createFile(technology, "testContainerErrorHandling.root", 'o');
  auto writeContainer = createWriteContainer(technology, "test/testData");

  std::vector<float> testData;
  void const* ptrTestData = &testData;
  auto const& typeInfo = typeid(testData);

  SECTION("fill() before setupWrite()")
  {
    CHECK_THROWS_AS(writeContainer->fill(ptrTestData), std::runtime_error);
  }

  SECTION("commit() before setupWrite()")
  {
    CHECK_THROWS_AS(writeContainer->commit(), std::runtime_error);
  }

  auto writeAssocContainer = dynamic_pointer_cast<Storage_Associative_Write_Container>(writeContainer);
  if(writeAssocContainer)
  {
    SECTION("fill() before setParent()")
    {
      CHECK_THROWS_AS(writeContainer->setupWrite(typeInfo), std::runtime_error);
      CHECK_THROWS_AS(writeContainer->fill(ptrTestData), std::runtime_error);
    }

    SECTION("commit() before setParent()")
    {
      CHECK_THROWS_AS(writeContainer->commit(), std::runtime_error);
    }

    SECTION("setupWrite() before setParent()")
    {
      CHECK_THROWS_AS(writeContainer->setupWrite(typeInfo), std::runtime_error);
    }

    auto parent = createWriteAssociation(technology, "test");
    parent->setFile(file);
    parent->setupWrite(typeInfo);
    if(form::technology::GetMinor(technology) != form::technology::ROOT_TTREE_MINOR) //TODO: dedicated TTree testing PR to fix this
    {
      SECTION("commit() before fill()")
      {
        writeAssocContainer->setParent(parent);
        writeContainer->setupWrite(typeInfo);
        CHECK_THROWS_AS(writeContainer->commit(), std::runtime_error);
      }
    }
  }

  auto readContainer = createReadContainer(technology, "test/testData");

  SECTION("read() before setParent()")
  {
    CHECK_THROWS_AS(readContainer->read(0, &ptrTestData, typeInfo), std::runtime_error);
  }

  SECTION("mismatched file type")
  {
    std::shared_ptr<IStorage_File> wrongFile(
      new Storage_File("testContainerErrorHandling.root", 'o'));
    CHECK_THROWS_AS(readContainer->setFile(wrongFile), std::runtime_error);
    CHECK_THROWS_AS(writeContainer->setFile(wrongFile), std::runtime_error);
  }

  auto associativeWrite = dynamic_pointer_cast<Storage_Associative_Write_Container>(writeContainer);
  if(associativeWrite)
  {
    SECTION("mismatched parent type")
    {
      std::shared_ptr<IStorage_Write_Container> badWriteParent(
        new Storage_Write_Container("bad"));
      CHECK_THROWS_AS(associativeWrite->setParent(badWriteParent), std::runtime_error);
    }
  }

  if(form::technology::GetMinor(technology) != form::technology::ROOT_TTREE_MINOR) //TODO: dedicated TTree testing PR to fix this)
  {
    SECTION("Bad file")
    {
      auto badFile = createFile(technology, "noSuchFile.root", 'i');
      CHECK_THROWS_AS(writeContainer->setFile(badFile), std::runtime_error);
    }
  }
}

template <class T>
void testFundamental(T const expected)
{
  SECTION(form::test::getTypeName<T>())
  {
    form::test::write(technology, expected);
    auto const [result] = form::test::read<T>(technology);
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }
}

// The switch in root_tbranch_read_container.cpp::read() handles all 13 ROOT
// fundamental EDataType values. Each SECTION below exercises one distinct case
// by writing a branch with the matching ROOT leaf-type character and reading it
// back through the FORM read container.
//
// The switch's `default:` branch is a defensive guard against future ROOT
// EDataType values not yet in the enumeration; it is not reachable with the
// current ROOT release and is therefore not tested here.
TEST_CASE("Root branch read: fundamental scalar types round-trip", "[form]")
{
  testFundamental(static_cast<char>('r'));
  testFundamental(static_cast<unsigned char>(200));
  testFundamental(static_cast<short>(-1000));
  testFundamental(static_cast<unsigned short>(60000));
  testFundamental(-42000);
  testFundamental(3000000000u);
  testFundamental(-9000000000L);
  testFundamental(9000000000UL);
  testFundamental(-4000000000LL);
  testFundamental(8000000000ULL);
  testFundamental(3.14f);
  testFundamental(2.718281828);
  testFundamental(true);
}

TEST_CASE("Root branch read: returns false when id exceeds entry count", "[form]")
{
  std::vector<int> data = {1, 2, 3};
  form::test::write(technology, data);

  auto file = createFile(technology, form::test::testFileName, 'i');
  auto container =
    createReadContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
  container->setFile(file);
  void const* rawPtr = nullptr;

  // One entry exists (id 0). id=2 strictly exceeds GetEntries()==1.
  CHECK_FALSE(container->read(2, &rawPtr, typeid(std::vector<int>)));
}

TEST_CASE("Root branch read: throws when the named tree is absent from the file", "[form]")
{
  std::vector<int> data = {42};
  form::test::write(technology, data);

  auto file = createFile(technology, form::test::testFileName, 'i');
  auto container = createReadContainer(technology, "NonExistentTree/someBranch");
  container->setFile(file);
  void const* rawPtr = nullptr;
  CHECK_THROWS_AS(container->read(0, &rawPtr, typeid(std::vector<int>)), std::runtime_error);
}

TEST_CASE("Root branch read: throws when the named branch is absent from the tree", "[form]")
{
  std::vector<int> data = {42};
  form::test::write(technology, data);

  auto file = createFile(technology, form::test::testFileName, 'i');
  auto container =
    createReadContainer(technology, std::string(form::test::testTreeName) + "/NonExistentBranch");
  container->setFile(file);
  void const* rawPtr = nullptr;
  CHECK_THROWS_AS(container->read(0, &rawPtr, typeid(std::vector<int>)), std::runtime_error);
}

TEST_CASE("Root branch read: throws for a type with no ROOT dictionary", "[form]")
{
  // A locally-defined struct has no ROOT reflection dictionary.
  // TDictionary::GetDictionary(typeid(LocalType)) returns nullptr, which
  // exercises the "unsupported type" error path in read().
  struct LocalType {};

  std::vector<int> data = {42};
  form::test::write(technology, data);

  auto file = createFile(technology, form::test::testFileName, 'i');
  auto container =
    createReadContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
  container->setFile(file);
  void const* rawPtr = nullptr;
  CHECK_THROWS_AS(container->read(0, &rawPtr, typeid(LocalType)), std::runtime_error);
}

TEST_CASE("Root TTree write container: fill and commit are not implemented", "[form]")
{
  auto file = createFile(technology, "testTTreeWriteOps.root", 'o');
  auto writeAssoc = createWriteAssociation(technology, "testTTreeWriteOpsTree");
  writeAssoc->setFile(file);
  writeAssoc->setupWrite();

  void const* dummy = nullptr;
  CHECK_THROWS_AS(writeAssoc->fill(dummy), std::runtime_error);
  CHECK_THROWS_AS(writeAssoc->commit(), std::runtime_error);
}

TEST_CASE("Unsupported attribute throws exception", "[form]")
{
  auto read_container =
     createReadContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
  CHECK_THROWS_AS(read_container->setAttribute("key", "value"), std::runtime_error);

  auto write_container =
    createWriteContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
  CHECK_THROWS_AS(write_container->setAttribute("key", "value"), std::runtime_error);
}
