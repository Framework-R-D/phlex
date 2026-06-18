//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include "core/placement.hpp"
#include "form/config.hpp"
#include "form/technology.hpp"
#include "root_storage/root_tbranch_write_container.hpp"
#include "root_storage/root_tfile.hpp"
#include "root_storage/root_ttree_write_container.hpp"
#include "storage/storage_file.hpp"
#include "storage/storage_write_container.hpp"
#include "storage/storage_writer.hpp"

#include "TFile.h"
#include "TTree.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numeric>
#include <string>
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

TEST_CASE("Root file open modes and attribute validation", "[form]")
{
  std::string const file_name = "testRootTFileModes.root";

  {
    auto seed = TFile::Open(file_name.c_str(), "RECREATE");
    REQUIRE(seed != nullptr);
    seed->Write();
    seed->Close();
  }

  SECTION("update mode reopens existing file")
  {
    ROOT_TFileImp file(file_name, 'u');
    REQUIRE(file.getTFile() != nullptr);
  }

  SECTION("update mode creates file when missing")
  {
    std::string const missing_name = "testRootTFileModesMissing.root";
    ROOT_TFileImp file(missing_name, 'u');
    REQUIRE(file.getTFile() != nullptr);
  }

  SECTION("read mode opens file")
  {
    ROOT_TFileImp file(file_name, 'r');
    REQUIRE(file.getTFile() != nullptr);
  }

  SECTION("unknown compression token keeps ROOT default")
  {
    ROOT_TFileImp file(file_name, 'o');
    int default_level = file.getTFile()->GetCompressionLevel();
    CHECK_NOTHROW(file.setAttribute("compression", "NotARealROOTCompressionAlgo"));
    CHECK(file.getTFile()->GetCompressionLevel() == default_level);
  }

  SECTION("unsupported attribute throws")
  {
    ROOT_TFileImp file(file_name, 'o');
    CHECK_THROWS_AS(file.setAttribute("does_not_exist", "1"), std::runtime_error);
  }

  SECTION("unsupported file mode throws")
  {
    CHECK_THROWS_AS(ROOT_TFileImp(file_name, 'z'), std::runtime_error);
  }
}

TEST_CASE("Storage write container error paths", "[form]")
{
  auto container = createWriteContainer(technology, "testTree");
  REQUIRE(container != nullptr);

  CHECK(container->getEntryCount() == 0);
  CHECK_THROWS_AS(container->setupWrite(typeid(int)), std::runtime_error);

  std::shared_ptr<IStorage_File> wrong_file(new Storage_File("container_error.root", 'o'));
  CHECK_THROWS_AS(container->setFile(wrong_file), std::runtime_error);
}

TEST_CASE("Storage write container default attribute rejection", "[form]")
{
  Storage_Write_Container container("test/container");
  CHECK_THROWS_AS(container.setAttribute("auto_flush", "1"), std::runtime_error);
}

TEST_CASE("Storage write container rejects unknown attributes", "[form]")
{
  auto container = createWriteContainer(technology, "test/branch");
  REQUIRE(container != nullptr);

  CHECK_THROWS_AS(container->setAttribute("not_supported", "1"), std::runtime_error);
}

TEST_CASE("StorageWriter finalize skips IndexRegistry for unparsable index", "[form]")
{
  std::string const file_name = "testStorageWriterInvalidIndex.root";
  StorageWriter writer;
  form::experimental::config::tech_setting_config settings;

  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  containers.emplace(std::make_unique<Placement>(file_name, "UnitTest/value", technology),
                     &typeid(std::vector<int>));
  containers.emplace(std::make_unique<Placement>(file_name, "UnitTest/index", technology),
                     &typeid(std::string));
  writer.createContainers(containers, settings);

  Placement payload_placement(file_name, "UnitTest/value", technology);
  std::vector<int> payload = {1, 2, 3};
  writer.fillContainer(payload_placement, &payload, typeid(std::vector<int>), "DeclaredProduct");

  Placement index_placement(file_name, "UnitTest/index", technology);
  std::string bad_index = "EVENT0001";
  writer.fillContainer(index_placement, &bad_index, typeid(std::string), "UnitTest");
  writer.commitContainers(index_placement);
  writer.finalize(settings);

  auto file = TFile::Open(file_name.c_str(), "READ");
  REQUIRE(file != nullptr);
  REQUIRE_FALSE(file->IsZombie());

  CHECK(file->Get<TTree>("FileCatalog") != nullptr);
  CHECK(file->Get<TTree>("ProductRegistry") != nullptr);
  CHECK(file->Get<TTree>("IndexRegistry") == nullptr);

  file->Close();
}

TEST_CASE("Storage_Write_Container base class exercises all default methods", "[form]")
{
  Storage_Write_Container c("my/container");

  // name() and getEntryCount() on base class
  CHECK(c.name() == "my/container");
  CHECK(c.getEntryCount() == 0u);

  // setFile, setupWrite, fill, commit are all no-ops on the base class
  auto dummy_file = std::make_shared<Storage_File>("dummy_base_test.root", 'o');
  CHECK_NOTHROW(c.setFile(dummy_file));
  CHECK_NOTHROW(c.setupWrite(typeid(int)));
  int val = 99;
  CHECK_NOTHROW(c.fill(&val));
  CHECK_NOTHROW(c.commit());
}

TEST_CASE("StorageWriter fillContainer throws when container is not registered", "[form]")
{
  StorageWriter writer;
  form::experimental::config::tech_setting_config settings;
  // No createContainers called, so m_write_containers is empty
  Placement p("ghost.root", "NoSuch/container", technology);
  int d = 1;
  CHECK_THROWS_AS(writer.fillContainer(p, &d, typeid(int), ""), std::runtime_error);
}

TEST_CASE("StorageWriter fillContainer with empty product_name falls back to creator_name",
          "[form]")
{
  std::string const file_name = "testEmptyProductName.root";
  StorageWriter writer;
  form::experimental::config::tech_setting_config settings;

  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  containers.emplace(std::make_unique<Placement>(file_name, "Creator/value", technology),
                     &typeid(std::vector<int>));
  containers.emplace(std::make_unique<Placement>(file_name, "Creator/index", technology),
                     &typeid(std::string));
  writer.createContainers(containers, settings);

  // Empty product_name → logical_product_name falls back to creator_name ("Creator")
  Placement pp(file_name, "Creator/value", technology);
  std::vector<int> d = {7};
  CHECK_NOTHROW(writer.fillContainer(pp, &d, typeid(std::vector<int>), ""));

  Placement ip(file_name, "Creator/index", technology);
  std::string idx = "[RUN=00000001;EVT=00000002]";
  writer.fillContainer(ip, &idx, typeid(std::string), "Creator");
  writer.commitContainers(ip);
  writer.finalize(settings);

  auto f = TFile::Open(file_name.c_str(), "READ");
  REQUIRE(f != nullptr);
  TTree* reg = f->Get<TTree>("ProductRegistry");
  REQUIRE(reg != nullptr);
  std::string* prod_name = nullptr;
  reg->SetBranchAddress("ProductName", &prod_name);
  reg->GetEntry(0);
  REQUIRE(prod_name != nullptr);
  // product_name was empty, so creator_name ("Creator") was used
  CHECK(*prod_name == "Creator");
  f->Close();
}

TEST_CASE("StorageWriter createContainers reuses existing parent TTree", "[form]")
{
  std::string const file_name = "testSharedParentTTree.root";
  StorageWriter writer;
  form::experimental::config::tech_setting_config settings;

  // First call: creates "SharedTree" parent + "branch1" container
  {
    std::map<std::unique_ptr<Placement>, std::type_info const*> c1;
    c1.emplace(std::make_unique<Placement>(file_name, "SharedTree/branch1", technology),
               &typeid(std::vector<int>));
    writer.createContainers(c1, settings);
  }

  // Second call: "SharedTree" parent already in m_write_containers → hits else branch
  {
    std::map<std::unique_ptr<Placement>, std::type_info const*> c2;
    c2.emplace(std::make_unique<Placement>(file_name, "SharedTree/branch2", technology),
               &typeid(float));
    CHECK_NOTHROW(writer.createContainers(c2, settings));
  }

  // Both branches must be usable without errors
  std::vector<int> d1 = {1, 2};
  float d2 = 2.71f;
  CHECK_NOTHROW(writer.fillContainer(
    Placement(file_name, "SharedTree/branch1", technology), &d1, typeid(std::vector<int>), "p1"));
  CHECK_NOTHROW(writer.fillContainer(
    Placement(file_name, "SharedTree/branch2", technology), &d2, typeid(float), "p2"));
  CHECK_NOTHROW(writer.finalize(settings));
}

TEST_CASE("StorageWriter fillContainer index with nullptr data skips index recording", "[form]")
{
  std::string const file_name = "testNullIndexData.root";
  StorageWriter writer;
  form::experimental::config::tech_setting_config settings;

  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  containers.emplace(std::make_unique<Placement>(file_name, "Prod/index", technology),
                     &typeid(std::string));
  writer.createContainers(containers, settings);

  Placement ip(file_name, "Prod/index", technology);
  // data == nullptr → is_index_container && data != nullptr evaluates false → inner block skipped
  CHECK_NOTHROW(writer.fillContainer(ip, nullptr, typeid(std::string), "Prod"));
  writer.commitContainers(ip);
  writer.finalize(settings);

  auto f = TFile::Open(file_name.c_str(), "READ");
  REQUIRE(f != nullptr);
  CHECK(f->Get<TTree>("FileCatalog") != nullptr);
  // No parsable index recorded → no IndexRegistry
  CHECK(f->Get<TTree>("IndexRegistry") == nullptr);
  f->Close();
}

TEST_CASE(
  "StorageWriter finalize with no payload: no ProductRegistry; index entry with empty fields",
  "[form]")
{
  std::string const file_name = "testNoPayloadProducts.root";
  StorageWriter writer;
  form::experimental::config::tech_setting_config settings;

  // Register only an index container — no payload container
  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  containers.emplace(std::make_unique<Placement>(file_name, "Anon/index", technology),
                     &typeid(std::string));
  writer.createContainers(containers, settings);

  // Fill index with valid parsable data; no payload registered for "Anon"
  // → products_for_index empty + m_productsByProducer empty → push empty strings into index
  Placement ip(file_name, "Anon/index", technology);
  std::string idx = "[RUN=00000001]";
  writer.fillContainer(ip, &idx, typeid(std::string), "Anon");
  writer.commitContainers(ip);
  writer.finalize(settings);

  auto f = TFile::Open(file_name.c_str(), "READ");
  REQUIRE(f != nullptr);
  CHECK(f->Get<TTree>("FileCatalog") != nullptr);
  // m_productsByProducer empty → no ProductRegistry
  CHECK(f->Get<TTree>("ProductRegistry") == nullptr);
  // Index data was recorded (with empty product info) → IndexRegistry is written
  CHECK(f->Get<TTree>("IndexRegistry") != nullptr);
  f->Close();
}

TEST_CASE("Root TTree setupWrite finds pre-existing TTree; getEntryCount reflects entries",
          "[form]")
{
  std::string const file_name = "testPreExistingTTree.root";

  // Step 1: create a file with a TTree that has some entries using plain ROOT
  {
    auto* seed_file = TFile::Open(file_name.c_str(), "RECREATE");
    REQUIRE(seed_file != nullptr);
    auto* tree = new TTree("ExistingTree", "seed");
    int x = 0;
    tree->Branch("x", &x, "x/I");
    for (x = 0; x < 3; ++x) {
      tree->Fill();
    }
    seed_file->Write();
    seed_file->Close();
  }

  // Step 2: open in update mode; setupWrite must find the existing TTree
  auto root_file = std::make_shared<ROOT_TFileImp>(file_name, 'u');
  ROOT_TTree_Write_ContainerImp container("ExistingTree");
  container.setFile(root_file);
  // setupWrite: first m_tree.reset(Get<TTree>()) returns non-null → skips new-tree branch
  CHECK_NOTHROW(container.setupWrite(typeid(void)));
  CHECK(container.getTTree() != nullptr);
  // getEntryCount() where m_tree != nullptr returns m_tree->GetEntries()
  CHECK(container.getEntryCount() == 3u);
}

TEST_CASE("StorageWriter parses colon indices and honors process_name key", "[form]")
{
  std::string const file_name = "testStorageWriterColonIndex.root";
  StorageWriter writer;
  form::experimental::config::tech_setting_config settings;

  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  containers.emplace(std::make_unique<Placement>(file_name, "UnitTest/value", technology),
                     &typeid(std::vector<int>));
  containers.emplace(std::make_unique<Placement>(file_name, "UnitTest/index", technology),
                     &typeid(std::string));
  writer.createContainers(containers, settings);

  Placement payload_placement(file_name, "UnitTest/value", technology);
  std::vector<int> payload = {10, 20, 30};
  writer.fillContainer(payload_placement, &payload, typeid(std::vector<int>), "DeclaredProduct");

  Placement index_placement(file_name, "UnitTest/index", technology);
  std::string index_text = "[EVENT:0000000A;SEG:0000000B]";
  writer.fillContainer(index_placement, &index_text, typeid(std::string), "UnitTest");
  writer.commitContainers(index_placement);
  writer.finalize(settings);

  auto file = TFile::Open(file_name.c_str(), "READ");
  REQUIRE(file != nullptr);
  REQUIRE_FALSE(file->IsZombie());

  TTree* registry = file->Get<TTree>("ProductRegistry");
  REQUIRE(registry != nullptr);
  REQUIRE(registry->GetEntries() > 0);
  std::string* process_name = nullptr;
  registry->SetBranchAddress("ProcessName", &process_name);
  registry->GetEntry(0);
  REQUIRE(process_name != nullptr);
  CHECK(process_name->empty());

  TTree* index_registry = file->Get<TTree>("IndexRegistry");
  REQUIRE(index_registry != nullptr);
  REQUIRE(index_registry->GetEntries() > 0);

  unsigned long long event_value = 0;
  unsigned long long seg_value = 0;
  std::string* product_id = nullptr;
  index_registry->SetBranchAddress("EVENT", &event_value);
  index_registry->SetBranchAddress("SEG", &seg_value);
  index_registry->SetBranchAddress("ProductID", &product_id);
  index_registry->GetEntry(0);

  CHECK(event_value == 10ULL);
  CHECK(seg_value == 11ULL);
  REQUIRE(product_id != nullptr);
  CHECK(*product_id == "DeclaredProduct|UnitTest|");

  file->Close();
}
