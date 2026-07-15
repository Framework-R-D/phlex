//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include "form/config.hpp"
#include "persistence/persistence_reader.hpp"
#include "persistence/persistence_writer.hpp"
#include "storage/storage_reader.hpp"

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

  auto writeAssocContainer =
    dynamic_pointer_cast<Storage_Associative_Write_Container>(writeContainer);
  if (writeAssocContainer) {
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
    if (form::technology::GetMinor(technology) !=
        form::technology::ROOT_TTREE_MINOR) //TODO: dedicated TTree testing PR to fix this
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
  if (associativeWrite) {
    SECTION("mismatched parent type")
    {
      std::shared_ptr<IStorage_Write_Container> badWriteParent(new Storage_Write_Container("bad"));
      CHECK_THROWS_AS(associativeWrite->setParent(badWriteParent), std::runtime_error);
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

TEST_CASE("Persistence round-trip: structured index normalization and listing", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name = "persistence_roundtrip_" + std::to_string(technology) + ".root";
  std::string const creator = "norm_creator";

  ItemConfig cfg;
  cfg.addItem("prod", file_name, technology);

  std::vector<int> first = {10, 20, 30};
  std::vector<int> second = {40, 50, 60};

  std::string const first_id = "[EVENT=00000001;SEG=00000002]";
  std::string const second_id = "[EVENT=00000003;SEG=00000004]";

  {
    auto writer = createPersistenceWriter();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configureTechSettings(tech_setting_config{});
    writer->createContainers(creator, {{"prod", &typeid(std::vector<int>)}});

    writer->registerWrite(creator, "prod", &first, typeid(std::vector<int>));
    writer->commitOutput(creator, first_id);

    writer->registerWrite(creator, "prod", &second, typeid(std::vector<int>));
    writer->commitOutput(creator, second_id);
  }

  auto reader = createPersistenceReader();
  REQUIRE(reader != nullptr);
  reader->configure(cfg);
  reader->configureTechSettings(tech_setting_config{});

  reader->prime(creator, "prod", typeid(std::vector<int>));

  auto indices = reader->listIndices(creator, "prod");
  REQUIRE_FALSE(indices.empty());

  void const* raw = nullptr;
  // Backends may canonicalize persisted index strings differently.
  // Use an index emitted by listIndices() to verify readback.
  reader->read(creator, "prod", indices.front(), &raw, typeid(std::vector<int>));
  auto const* read_first = static_cast<std::vector<int> const*>(raw);
  REQUIRE(read_first != nullptr);
  CHECK((*read_first == first || *read_first == second));
}

TEST_CASE("Persistence round-trip: all-zero structured id fallback", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name = "persistence_zero_index_" + std::to_string(technology) + ".root";
  std::string const creator = "zero_creator";

  ItemConfig cfg;
  cfg.addItem("prod", file_name, technology);

  std::vector<int> payload = {7, 8, 9};
  {
    auto writer = createPersistenceWriter();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configureTechSettings(tech_setting_config{});
    writer->createContainers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->registerWrite(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commitOutput(creator, "");
  }

  auto reader = createPersistenceReader();
  REQUIRE(reader != nullptr);
  reader->configure(cfg);
  reader->configureTechSettings(tech_setting_config{});

  void const* raw = nullptr;
  reader->read(creator, "prod", "[EVENT=00000000;SEG=00000000]", &raw, typeid(std::vector<int>));

  auto const* read_payload = static_cast<std::vector<int> const*>(raw);
  REQUIRE(read_payload != nullptr);
  CHECK(*read_payload == payload);
}

TEST_CASE("StorageReader getIndex: malformed ids and compatibility fallbacks", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name =
    "storage_reader_index_branches_" + std::to_string(technology) + ".root";
  std::string const creator = "storage_reader_creator";
  std::string const index_container = creator + "/index";

  ItemConfig cfg;
  cfg.addItem("prod", file_name, technology);

  std::vector<int> payload = {1, 2, 3};
  {
    auto writer = createPersistenceWriter();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configureTechSettings(tech_setting_config{});
    writer->createContainers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->registerWrite(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commitOutput(creator, "[EVENT=00000001;SEG=00000002]");
  }

  StorageReader reader;
  Token const index_token{file_name, index_container, technology};
  tech_setting_config const settings{};

  CHECK(reader.getIndex(index_token, "[]", settings) == 0);
  CHECK(reader.getIndex(index_token, "plain-text-id", settings) == 0);

  CHECK_THROWS_AS(reader.getIndex(index_token, "[EVENT,SEG=00000001]", settings),
                  std::runtime_error);
  CHECK_THROWS_AS(
    reader.getIndex(index_token, "[EVENT=99999999999999999999999999999999]", settings),
    std::runtime_error);
  CHECK_THROWS_AS(reader.getIndex(index_token, "[=00000001]", settings), std::runtime_error);
  CHECK_THROWS_AS(reader.getIndex(index_token, "[EVENT]", settings), std::runtime_error);
  CHECK_THROWS_AS(reader.getIndex(index_token, "[    ]", settings), std::runtime_error);
}

TEST_CASE("StorageReader getIndex: empty container and tech-table branches", "[form]")
{
  using namespace form::experimental::config;

  StorageReader reader;
  Token const token{"storage_reader_hdf5_get_index.root", "creator/index", form::technology::HDF5};

  tech_setting_config empty_settings;
  CHECK_THROWS_AS(reader.getIndex(token, "[EVENT=00000001;SEG=00000001]", empty_settings),
                  std::runtime_error);

  tech_setting_config tech_only_settings;
  tech_only_settings.file_settings[form::technology::HDF5]["different_file"] = {};
  tech_only_settings.container_settings[form::technology::HDF5]["different_container"] = {};
  CHECK_THROWS_AS(reader.getIndex(token, "[EVENT=00000001;SEG=00000001]", tech_only_settings),
                  std::runtime_error);

  std::string const file_name =
    "storage_reader_getindex_attr_" + std::to_string(technology) + ".root";
  std::string const creator = "storage_reader_getindex_attr_creator";
  ItemConfig cfg;
  cfg.addItem("prod", file_name, technology);
  std::vector<int> payload = {5, 6, 7};
  {
    auto writer = createPersistenceWriter();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configureTechSettings(tech_setting_config{});
    writer->createContainers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->registerWrite(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commitOutput(creator, "[EVENT=00000005;SEG=00000006]");
  }

  tech_setting_config attr_settings;
  attr_settings.file_settings[technology][file_name] = {{"compression", "1"}};
  CHECK(reader.getIndex(
          Token{file_name, creator + "/index", technology}, "missing-id", attr_settings) == 0);

  tech_setting_config container_attr_settings;
  container_attr_settings.container_settings[technology][creator + "/index"] = {{"split", "0"}};
  CHECK_NOTHROW(reader.getIndex(
    Token{file_name, creator + "/index", technology}, "missing-id", container_attr_settings));
}

TEST_CASE("StorageReader prime/listIndices/readContainer: attribute and error branches", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name = "storage_reader_misc_attr_" + std::to_string(technology) + ".root";
  std::string const creator = "storage_reader_misc_creator";
  ItemConfig cfg;
  cfg.addItem("prod", file_name, technology);
  std::vector<int> payload = {9, 8, 7};
  {
    auto writer = createPersistenceWriter();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configureTechSettings(tech_setting_config{});
    writer->createContainers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->registerWrite(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commitOutput(creator, "[EVENT=00000009;SEG=00000008]");
  }

  StorageReader reader;
  Token const index_token{file_name, creator + "/index", technology};

  tech_setting_config file_attr_settings;
  file_attr_settings.file_settings[technology][file_name] = {{"compression", "1"}};

  CHECK_NOTHROW(reader.prime(index_token, typeid(std::string), file_attr_settings));

  void const* raw = nullptr;
  CHECK_NOTHROW(reader.readContainer(Token{file_name, creator + "/prod", technology, 0},
                                     &raw,
                                     typeid(std::vector<int>),
                                     file_attr_settings));

  tech_setting_config empty_settings;
  CHECK_THROWS_AS(reader.listIndices(
                    Token{"storage_reader_hdf5_misc.root", "creator/index", form::technology::HDF5},
                    empty_settings),
                  std::runtime_error);

  tech_setting_config container_attr_settings;
  container_attr_settings.container_settings[technology][creator + "/index"] = {{"split", "0"}};

  CHECK_NOTHROW(reader.listIndices(index_token, container_attr_settings));
  CHECK_NOTHROW(reader.readContainer(Token{file_name, creator + "/prod", technology, 0},
                                     &raw,
                                     typeid(std::vector<int>),
                                     container_attr_settings));
}

TEST_CASE("Root branch prime: error paths", "[form]")
{
  SECTION("no file attached throws")
  {
    auto container = createReadContainer(technology, "SomeTree/branch");
    CHECK_THROWS_AS(container->prime(typeid(std::vector<int>)), std::runtime_error);
  }

  SECTION("container name not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = createFile(technology, form::test::testFileName, 'i');
    auto container = createReadContainer(technology, "NonExistentTreeForPrime/branch");
    container->setFile(file);
    CHECK_THROWS_AS(container->prime(typeid(std::vector<int>)), std::runtime_error);
  }

  SECTION("branch not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = createFile(technology, form::test::testFileName, 'i');
    auto container = createReadContainer(
      technology, std::string(form::test::testTreeName) + "/NonExistentBranchForPrime");
    container->setFile(file);
    CHECK_THROWS_AS(container->prime(typeid(std::vector<int>)), std::runtime_error);
  }

  SECTION("unsupported type throws")
  {
    struct LocalPrimeType {};
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = createFile(technology, form::test::testFileName, 'i');
    auto container =
      createReadContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
    container->setFile(file);
    CHECK_THROWS_AS(container->prime(typeid(LocalPrimeType)), std::runtime_error);
  }
}

TEST_CASE("Root branch entries: success and error paths", "[form]")
{
  SECTION("no file attached throws")
  {
    auto container = createReadContainer(technology, "SomeTree/branch");
    CHECK_THROWS_AS(container->entries(), std::runtime_error);
  }

  SECTION("tree not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = createFile(technology, form::test::testFileName, 'i');
    auto container = createReadContainer(technology, "NonExistentTreeForEntries/branch");
    container->setFile(file);
    CHECK_THROWS_AS(container->entries(), std::runtime_error);
  }

  SECTION("branch not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = createFile(technology, form::test::testFileName, 'i');
    auto container = createReadContainer(
      technology, std::string(form::test::testTreeName) + "/NonExistentBranchForEntries");
    container->setFile(file);
    CHECK_THROWS_AS(container->entries(), std::runtime_error);
  }

  SECTION("valid container returns entry count")
  {
    std::vector<int> data = {10, 20, 30};
    form::test::write(technology, data);
    auto file = createFile(technology, form::test::testFileName, 'i');
    auto container =
      createReadContainer(technology, form::test::makeTestBranchName<std::vector<int>>());
    container->setFile(file);
    CHECK(container->entries() == 1);
  }
}
