//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include "TFile.h"
#include "TTree.h"

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
  auto container =
    createReadContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
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
  auto writeContainer = createWriteContainer(technology, "test/testData");

  std::vector<float> testData;
  void const* ptrTestData = &testData;
  auto const& typeInfo = typeid(testData);

  SECTION("fill() before setParent()")
  {
    CHECK_THROWS_AS(writeContainer->setupWrite(typeInfo), std::runtime_error);
    CHECK_THROWS_AS(writeContainer->fill(ptrTestData), std::runtime_error);
  }

  SECTION("commit() before setParent()")
  {
    CHECK_THROWS_AS(writeContainer->commit(), std::runtime_error);
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
}

// ============================================================
// Helpers for fundamental scalar type round-trip tests.
//
// test_utils.hpp uses TClass::GetClass<T>(), which returns nullptr
// for fundamental types (int, float, etc.), so we bypass it and write
// branches directly via the ROOT API with an explicit leaf-type character.
// This guarantees the on-disk EDataType matches the switch case we intend
// to exercise in root_tbranch_read_container.cpp::read().
// ============================================================
namespace {
  char const* const kFundTestTree = "FundTestTree";

  template <typename T>
  void writeFundamentalDirect(std::string const& fileName,
                              std::string const& branchName,
                              std::string const& leafSpec,
                              T value)
  {
    TFile f(fileName.c_str(), "RECREATE");
    TTree t(kFundTestTree, kFundTestTree);
    T val = value;
    t.Branch(branchName.c_str(), &val, (branchName + leafSpec).c_str());
    t.Fill();
    f.Write();
  }

  template <typename T>
  std::unique_ptr<T const> readFundamental(std::string const& fileName,
                                           std::string const& branchName)
  {
    auto const technology = form::technology::ROOT_TTREE;
    auto file = createFile(technology, fileName, 'i');
    auto container = createReadContainer(technology, std::string(kFundTestTree) + "/" + branchName);
    container->setFile(file);
    void const* rawPtr = nullptr;
    if (!container->read(0, &rawPtr, typeid(T)))
      return nullptr;
    return std::unique_ptr<T const>(static_cast<T const*>(rawPtr));
  }
} // namespace

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
  SECTION("Char_t — kChar_t, leaf type /B")
  {
    Char_t const expected = 42;
    writeFundamentalDirect<Char_t>("fund_char.root", "val", "/B", expected);
    auto result = readFundamental<Char_t>("fund_char.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("UChar_t — kUChar_t, leaf type /b")
  {
    UChar_t const expected = 200;
    writeFundamentalDirect<UChar_t>("fund_uchar.root", "val", "/b", expected);
    auto result = readFundamental<UChar_t>("fund_uchar.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("Short_t — kShort_t, leaf type /S")
  {
    Short_t const expected = -1000;
    writeFundamentalDirect<Short_t>("fund_short.root", "val", "/S", expected);
    auto result = readFundamental<Short_t>("fund_short.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("UShort_t — kUShort_t, leaf type /s")
  {
    UShort_t const expected = 60000;
    writeFundamentalDirect<UShort_t>("fund_ushort.root", "val", "/s", expected);
    auto result = readFundamental<UShort_t>("fund_ushort.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("Int_t — kInt_t, leaf type /I")
  {
    Int_t const expected = -42000;
    writeFundamentalDirect<Int_t>("fund_int.root", "val", "/I", expected);
    auto result = readFundamental<Int_t>("fund_int.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("UInt_t — kUInt_t, leaf type /i")
  {
    UInt_t const expected = 3000000000u;
    writeFundamentalDirect<UInt_t>("fund_uint.root", "val", "/i", expected);
    auto result = readFundamental<UInt_t>("fund_uint.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("Long_t — kLong_t, leaf type /G")
  {
    Long_t const expected = -9000000000L;
    writeFundamentalDirect<Long_t>("fund_long.root", "val", "/G", expected);
    auto result = readFundamental<Long_t>("fund_long.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("ULong_t — kULong_t, leaf type /g")
  {
    ULong_t const expected = 9000000000UL;
    writeFundamentalDirect<ULong_t>("fund_ulong.root", "val", "/g", expected);
    auto result = readFundamental<ULong_t>("fund_ulong.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("Long64_t — kLong64_t, leaf type /L")
  {
    Long64_t const expected = -4000000000LL;
    writeFundamentalDirect<Long64_t>("fund_long64.root", "val", "/L", expected);
    auto result = readFundamental<Long64_t>("fund_long64.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("ULong64_t — kULong64_t, leaf type /l")
  {
    ULong64_t const expected = 8000000000ULL;
    writeFundamentalDirect<ULong64_t>("fund_ulong64.root", "val", "/l", expected);
    auto result = readFundamental<ULong64_t>("fund_ulong64.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("Float_t — kFloat_t, leaf type /F")
  {
    Float_t const expected = 3.14f;
    writeFundamentalDirect<Float_t>("fund_float.root", "val", "/F", expected);
    auto result = readFundamental<Float_t>("fund_float.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("Double_t — kDouble_t, leaf type /D")
  {
    Double_t const expected = 2.718281828;
    writeFundamentalDirect<Double_t>("fund_double.root", "val", "/D", expected);
    auto result = readFundamental<Double_t>("fund_double.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }

  SECTION("Bool_t — kBool_t, leaf type /O")
  {
    Bool_t const expected = true;
    writeFundamentalDirect<Bool_t>("fund_bool.root", "val", "/O", expected);
    auto result = readFundamental<Bool_t>("fund_bool.root", "val");
    REQUIRE(result != nullptr);
    CHECK(*result == expected);
  }
}

TEST_CASE("Root branch read: returns false when id exceeds entry count", "[form]")
{
  int const technology = form::technology::ROOT_TTREE;
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
  int const technology = form::technology::ROOT_TTREE;
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
  int const technology = form::technology::ROOT_TTREE;
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

  int const technology = form::technology::ROOT_TTREE;
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
  int const technology = form::technology::ROOT_TTREE;
  auto file = createFile(technology, "testTTreeWriteOps.root", 'o');
  auto writeAssoc = createWriteAssociation(technology, "testTTreeWriteOpsTree");
  writeAssoc->setFile(file);
  writeAssoc->setupWrite();

  void const* dummy = nullptr;
  CHECK_THROWS_AS(writeAssoc->fill(dummy), std::runtime_error);
  CHECK_THROWS_AS(writeAssoc->commit(), std::runtime_error);
}
