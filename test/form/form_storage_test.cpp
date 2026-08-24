//Tests for FORM's storage layer's design requirements

#include "test/form/test_utils.hpp"

#include "form/config.hpp"
#include "persistence/persistence_reader.hpp"
#include "persistence/persistence_writer.hpp"
#include "root_storage/root_tfile.hpp"
#include "root_storage/root_ttree_write_container.hpp"
#include "storage/storage_file.hpp"
#include "storage/storage_reader.hpp"
#include "storage/storage_write_container.hpp"

#include "TBranch.h"
#include "TFile.h"
#include "TTree.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <numbers>
#include <numeric>
#include <vector>

using namespace form::detail::experimental;

namespace {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  form::technology::id technology = form::technology::root_ttree; //Potentially overridden in main
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

  int const return_code = session.applyCommandLine(argc, argv);
  if (return_code != 0) {
    return return_code;
  }

  technology = form::test::get_technology(tech_string);

  return session.run();
}

TEST_CASE("storage_container read wrong type", "[form]")
{
  std::vector<int> primes = {2, 3, 5, 7, 11, 13, 17, 19};
  form::test::write(technology, primes);

  auto file = create_file(technology, form::test::test_file_name, 'i');
  auto container =
    create_read_container(technology, form::test::make_test_branch_name<std::vector<int>>());
  container->set_file(file);
  void const* data_ptr = nullptr;
  CHECK_THROWS_AS(container->read(0, &data_ptr, typeid(double)), std::runtime_error);
}

TEST_CASE("storage_container sharing an Association", "[form]")
{
  std::vector<float> pi_data(10, std::numbers::pi_v<float>);
  std::string index_data = "[event:1, segment:1]";

  form::test::write(technology, pi_data, index_data);

  auto [pi_result, index_result] = form::test::read<std::vector<float>, std::string>(technology);

  SECTION("float container") { CHECK(*pi_result == pi_data); }

  SECTION("index") { CHECK(*index_result == index_data); }
}

TEST_CASE("storage_container multiple containers in Association", "[form]")
{
  std::vector<float> pi_data(10, std::numbers::pi_v<float>);
  std::vector<int> magic_data(17);
  std::ranges::iota(magic_data, 42);
  std::string index_data = "[event:1, segment:1]";

  form::test::write(technology, pi_data, magic_data, index_data);

  auto [pi_result, magic_result, index_result] =
    form::test::read<std::vector<float>, std::vector<int>, std::string>(technology);

  SECTION("float container") { CHECK(*pi_result == pi_data); }

  SECTION("int container") { CHECK(*magic_result == magic_data); }

  SECTION("index data") { CHECK(*index_result == index_data); }
}

TEST_CASE("FORM Container setup error handling")
{
  auto file = create_file(technology, "testContainerErrorHandling.root", 'o');
  auto write_container = create_write_container(technology, "test/test_data");

  std::vector<float> test_data;
  void const* ptr_test_data = &test_data;
  auto const& type_info = typeid(test_data);

  SECTION("fill() before setup_write()")
  {
    CHECK_THROWS_AS(write_container->fill(ptr_test_data), std::runtime_error);
  }

  SECTION("commit() before setup_write()")
  {
    CHECK_THROWS_AS(write_container->commit(), std::runtime_error);
  }

  auto write_assoc_container =
    dynamic_pointer_cast<storage_associative_write_container>(write_container);
  if (write_assoc_container) {
    SECTION("fill() before set_parent()")
    {
      CHECK_THROWS_AS(write_container->setup_write(type_info), std::runtime_error);
      CHECK_THROWS_AS(write_container->fill(ptr_test_data), std::runtime_error);
    }

    SECTION("commit() before set_parent()")
    {
      CHECK_THROWS_AS(write_container->commit(), std::runtime_error);
    }

    SECTION("setup_write() before set_parent()")
    {
      CHECK_THROWS_AS(write_container->setup_write(type_info), std::runtime_error);
    }

    auto parent = create_write_association(technology, "test");
    parent->set_file(file);
    parent->setup_write(type_info);
    SECTION("commit() before fill() without setup_write()")
    {
      write_assoc_container->set_parent(parent);
      CHECK_THROWS_AS(write_container->commit(), std::runtime_error);
    }

    SECTION("commit() before fill() with setup_write()")
    {
      write_assoc_container->set_parent(parent);
      write_container->setup_write(type_info);
      CHECK_THROWS_AS(write_container->commit(), std::runtime_error);
    }
  }

  auto read_container = create_read_container(technology, "test/test_data");

  SECTION("read() before set_parent()")
  {
    CHECK_THROWS_AS(read_container->read(0, &ptr_test_data, type_info), std::runtime_error);
  }

  SECTION("mismatched file type")
  {
    std::shared_ptr<i_storage_file> wrong_file(
      new storage_file("testContainerErrorHandling.root", 'o'));
    CHECK_THROWS_AS(read_container->set_file(wrong_file), std::runtime_error);
    CHECK_THROWS_AS(write_container->set_file(wrong_file), std::runtime_error);
  }

  auto associative_write =
    dynamic_pointer_cast<storage_associative_write_container>(write_container);
  if (associative_write) {
    SECTION("mismatched parent type")
    {
      std::shared_ptr<i_storage_write_container> bad_write_parent(
        new storage_write_container("bad"));
      CHECK_THROWS_AS(associative_write->set_parent(bad_write_parent), std::runtime_error);
    }
  }
}

template <class T>
void test_fundamental(T const expected)
{
  SECTION(form::test::get_type_name<T>())
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
  test_fundamental('r');
  test_fundamental(static_cast<unsigned char>(200));
  test_fundamental(static_cast<short>(-1000));
  test_fundamental(static_cast<unsigned short>(60000));
  test_fundamental(-42000);
  test_fundamental(3000000000u);
  test_fundamental(-9000000000L);
  test_fundamental(9000000000UL);
  test_fundamental(-4000000000LL);
  test_fundamental(8000000000ULL);
  test_fundamental(std::numbers::pi_v<float>);
  test_fundamental(std::numbers::e);
  test_fundamental(true);
}

TEST_CASE("Root branch read: returns false when id exceeds entry count", "[form]")
{
  std::vector<int> data = {1, 2, 3};
  form::test::write(technology, data);

  auto file = create_file(technology, form::test::test_file_name, 'i');
  auto container =
    create_read_container(technology, form::test::make_test_branch_name<std::vector<int>>());
  container->set_file(file);
  void const* raw_ptr = nullptr;

  // One entry exists (id 0). id=2 strictly exceeds GetEntries()==1.
  CHECK_FALSE(container->read(2, &raw_ptr, typeid(std::vector<int>)));
}

TEST_CASE("Root branch read: throws when the named tree is absent from the file", "[form]")
{
  std::vector<int> data = {42};
  form::test::write(technology, data);

  auto file = create_file(technology, form::test::test_file_name, 'i');
  auto container = create_read_container(technology, "NonExistentTree/someBranch");
  container->set_file(file);
  void const* raw_ptr = nullptr;
  CHECK_THROWS_AS(container->read(0, &raw_ptr, typeid(std::vector<int>)), std::runtime_error);
}

TEST_CASE("Root branch read: throws when the named branch is absent from the tree", "[form]")
{
  std::vector<int> data = {42};
  form::test::write(technology, data);

  auto file = create_file(technology, form::test::test_file_name, 'i');
  auto container = create_read_container(
    technology, std::string(form::test::test_tree_name) + "/NonExistentBranch");
  container->set_file(file);
  void const* raw_ptr = nullptr;
  CHECK_THROWS_AS(container->read(0, &raw_ptr, typeid(std::vector<int>)), std::runtime_error);
}

TEST_CASE("Root branch read: throws for a type with no ROOT dictionary", "[form]")
{
  // A locally-defined struct has no ROOT reflection dictionary.
  // TDictionary::GetDictionary(typeid(local_type)) returns nullptr, which
  // exercises the "unsupported type" error path in read().
  struct local_type {};

  std::vector<int> data = {42};
  form::test::write(technology, data);

  auto file = create_file(technology, form::test::test_file_name, 'i');
  auto container =
    create_read_container(technology, form::test::make_test_branch_name<std::vector<int>>());
  container->set_file(file);
  void const* raw_ptr = nullptr;
  CHECK_THROWS_AS(container->read(0, &raw_ptr, typeid(local_type)), std::runtime_error);
}

TEST_CASE("Root TTree write container: fill and commit are not implemented", "[form]")
{
  auto file = create_file(technology, "testTTreeWriteOps.root", 'o');
  auto write_assoc = create_write_association(technology, "testTTreeWriteOpsTree");
  write_assoc->set_file(file);
  write_assoc->setup_write();

  void const* dummy = nullptr;
  CHECK_THROWS_AS(write_assoc->fill(dummy), std::runtime_error);
  CHECK_THROWS_AS(write_assoc->commit(), std::runtime_error);
}

TEST_CASE("Root TBranch fill: throws when TBranch::Fill() reports a write error", "[form]")
{
  // Exercises the defensive guard in root_tbranch_write_container_imp::fill():
  // TBranch::Fill() returns a negative value when a basket flush to disk fails, and
  // fill() must throw rather than hand back a row id for data that was never persisted.
  //
  // TBranch is ROOT_TTREE-specific, so this test hard-codes that technology instead of
  // using the (CLI-overridable) global `technology`, which may select ROOT_RNTUPLE.
  //
  // To provoke a deterministic write failure we (1) shrink the branch basket so that a
  // handful of fills force a basket flush to disk, and (2) mark the underlying TFile
  // non-writable so that flush fails and Fill() returns a negative value.
  auto const tech = form::technology::root_ttree;

  auto file = create_file(tech, "tbranch_fill_write_error.root", 'o');
  auto tree = create_write_association(tech, "faketree");
  auto branch = create_write_container(tech, "faketree/fakebranch");

  tree->set_file(file);
  tree->setup_write(typeid(double));

  auto branch_assoc = dynamic_pointer_cast<storage_associative_write_container>(branch);
  REQUIRE(branch_assoc != nullptr);
  branch_assoc->set_parent(tree);
  branch->set_file(file);
  branch->setup_write(typeid(double));

  // Reach the raw ROOT objects created through the factory wiring above.
  auto root_file = dynamic_pointer_cast<root_tfile_imp>(file);
  REQUIRE(root_file != nullptr);
  auto* root_tree = dynamic_cast<root_ttree_write_container_imp*>(tree.get());
  REQUIRE(root_tree != nullptr);

  TTree* raw_tree = root_tree->get_ttree();
  REQUIRE(raw_tree != nullptr);
  TBranch* raw_branch = raw_tree->GetBranch("fakebranch");
  REQUIRE(raw_branch != nullptr);

  // A tiny basket forces a flush after only a few fills. ROOT clamps the minimum to 100
  // bytes, so a handful of 8-byte doubles is enough to overflow it.
  raw_branch->SetBasketSize(100);

  // Make the file non-writable so the forced basket flush fails.
  std::shared_ptr<TFile> raw_tfile = root_file->get_tfile();
  REQUIRE(raw_tfile != nullptr);
  raw_tfile->SetWritable(false);

  // Keep filling until a basket flush is triggered; the flush cannot reach the read-only
  // file, TBranch::Fill() returns a negative value, and fill() surfaces it as a throw.
  double value = std::numbers::pi;
  CHECK_THROWS_AS(
    [&] {
      for (int i = 0; i < 100000; ++i) {
        branch->fill(&value);
      }
    }(),
    std::runtime_error);

  // Restore writability so container teardown (which writes the tree) does not error.
  raw_tfile->SetWritable(true);
}

TEST_CASE("Persistence round-trip: structured index normalization and listing", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name =
    "persistence_roundtrip_" + form::technology::to_string(technology) + ".root";
  std::string const creator = "norm_creator";

  item_config cfg;
  cfg.add_item("prod", file_name, technology);

  std::vector<int> first = {10, 20, 30};
  std::vector<int> second = {40, 50, 60};

  std::string const first_id = "[event:1, segment:2]";
  std::string const second_id = "[event:3, segment:4]";

  {
    auto writer = create_persistence_writer();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configure_tech_settings(tech_setting_config{});
    writer->create_containers(creator, {{"prod", &typeid(std::vector<int>)}});

    writer->register_write(creator, "prod", &first, typeid(std::vector<int>));
    writer->commit_output(creator, first_id);

    writer->register_write(creator, "prod", &second, typeid(std::vector<int>));
    writer->commit_output(creator, second_id);
  }

  auto reader = create_persistence_reader();
  REQUIRE(reader != nullptr);
  reader->configure(cfg);
  reader->configure_tech_settings(tech_setting_config{});

  reader->prime(creator, "prod", typeid(std::vector<int>));

  auto indices = reader->list_indices(creator, "prod");
  REQUIRE_FALSE(indices.empty());

  void const* raw = nullptr;
  // Backends may canonicalize persisted index strings differently.
  // Use an index emitted by list_indices() to verify readback.
  reader->read(creator, "prod", indices.front(), &raw, typeid(std::vector<int>));
  auto const* read_first = static_cast<std::vector<int> const*>(raw);
  REQUIRE(read_first != nullptr);
  CHECK((*read_first == first || *read_first == second));
}

TEST_CASE("register_write returns a token locating the written product", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name =
    "registerwrite_rowid_" + form::technology::to_string(technology) + ".root";
  std::string const creator = "rowid_creator";
  std::string const container = creator + "/prod";

  item_config cfg;
  cfg.add_item("prod", file_name, technology);

  std::vector<int> const first = {11, 22, 33};
  std::vector<int> const second = {44, 55, 66};

  token token_first;
  token token_second;
  {
    auto writer = create_persistence_writer();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configure_tech_settings(tech_setting_config{});
    writer->create_containers(creator, {{"prod", &typeid(std::vector<int>)}});

    token_first = writer->register_write(creator, "prod", &first, typeid(std::vector<int>));
    writer->commit_output(creator, "[event:1, segment:1]");

    token_second = writer->register_write(creator, "prod", &second, typeid(std::vector<int>));
    writer->commit_output(creator, "[event:1, segment:2]");
  }

  // The returned token carries the placement and the 0-based, monotonically increasing row
  CHECK(token_first.has_id());
  CHECK(token_first.id() == 0u);
  CHECK(token_first.container_name() == container);
  CHECK(token_second.has_id());
  CHECK(token_second.id() == 1u);

  // token returned by the write is directly usable on the read side: no hand-buit token or re-scan
  storage_reader reader;
  tech_setting_config const settings{};

  // read_container allocates the payload and transfers ownership to the caller.
  void const* raw = nullptr;
  reader.read_container(token_first, &raw, typeid(std::vector<int>), settings);
  std::unique_ptr<std::vector<int> const> const got_first(
    static_cast<std::vector<int> const*>(raw));
  REQUIRE(got_first != nullptr);
  CHECK(*got_first == first);

  raw = nullptr;
  reader.read_container(token_second, &raw, typeid(std::vector<int>), settings);
  std::unique_ptr<std::vector<int> const> const got_second(
    static_cast<std::vector<int> const*>(raw));
  REQUIRE(got_second != nullptr);
  CHECK(*got_second == second);
}

TEST_CASE("register_write throws when the backend does not address rows", "[form]")
{
  using namespace form::experimental::config;

  // The generic ("no technology specified") backend's write container is a no-op whose fill()
  // returns invalid_row_id, and its read side is a no-op too, so a product routed there could
  // never be located on read. register_write must reject that rather than return an unusable
  // token whose row would later be used as the read-side navigation key.
  form::technology::id const generic{};
  std::string const file_name = "registerwrite_notset_row.generic";
  std::string const creator = "notset_creator";

  item_config cfg;
  cfg.add_item("prod", file_name, generic);

  std::vector<int> const payload = {1, 2, 3};

  auto writer = create_persistence_writer();
  REQUIRE(writer != nullptr);
  writer->configure(cfg);
  writer->configure_tech_settings(tech_setting_config{});
  writer->create_containers(creator, {{"prod", &typeid(std::vector<int>)}});

  CHECK_THROWS_AS(writer->register_write(creator, "prod", &payload, typeid(std::vector<int>)),
                  std::runtime_error);
}

TEST_CASE("Persistence round-trip: all-zero structured id fallback", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name =
    "persistence_zero_index_" + form::technology::to_string(technology) + ".root";
  std::string const creator = "zero_creator";

  item_config cfg;
  cfg.add_item("prod", file_name, technology);

  std::vector<int> payload = {7, 8, 9};
  {
    auto writer = create_persistence_writer();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configure_tech_settings(tech_setting_config{});
    writer->create_containers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->register_write(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commit_output(creator, "");
  }

  auto reader = create_persistence_reader();
  REQUIRE(reader != nullptr);
  reader->configure(cfg);
  reader->configure_tech_settings(tech_setting_config{});

  void const* raw = nullptr;
  reader->read(creator, "prod", "[event:0, segment:0]", &raw, typeid(std::vector<int>));

  auto const* read_payload = static_cast<std::vector<int> const*>(raw);
  REQUIRE(read_payload != nullptr);
  CHECK(*read_payload == payload);
}

TEST_CASE("storage_reader get_index: malformed ids and compatibility fallbacks", "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name =
    "storage_reader_index_branches_" + form::technology::to_string(technology) + ".root";
  std::string const creator = "storage_reader_creator";
  std::string const index_container = creator + "/index";

  item_config cfg;
  cfg.add_item("prod", file_name, technology);

  std::vector<int> payload = {1, 2, 3};
  {
    auto writer = create_persistence_writer();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configure_tech_settings(tech_setting_config{});
    writer->create_containers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->register_write(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commit_output(creator, "[event:1, segment:2]");
  }

  storage_reader reader;
  token const index_token{file_name, index_container, technology};
  tech_setting_config const settings{};

  CHECK(reader.get_index(index_token, "[]", settings) == 0);
  CHECK(reader.get_index(index_token, "plain-text-id", settings) == 0);

  // Malformed IDs must be rejected by the storage-layer index parser
  CHECK_THROWS_AS(reader.get_index(index_token, "[EVENT,SEG=1]", settings), std::runtime_error);
  CHECK_THROWS_AS(
    reader.get_index(index_token, "[EVENT=99999999999999999999999999999999]", settings),
    std::runtime_error);
  CHECK_THROWS_AS(reader.get_index(index_token, "[=1]", settings), std::runtime_error);
  CHECK_THROWS_AS(reader.get_index(index_token, "[EVENT]", settings), std::runtime_error);
  CHECK_THROWS_AS(reader.get_index(index_token, "[    ]", settings), std::runtime_error);
}

TEST_CASE("storage_reader get_index: empty container and tech-table branches", "[form]")
{
  using namespace form::experimental::config;

  storage_reader reader;
  token const index_token{
    "storage_reader_hdf5_get_index.root", "creator/index", form::technology::hdf5};

  tech_setting_config empty_settings;
  CHECK_THROWS_AS(reader.get_index(index_token, "[event:1, segment:1]", empty_settings),
                  std::runtime_error);

  tech_setting_config tech_only_settings;
  tech_only_settings.file_settings[form::technology::hdf5]["different_file"] = {};
  tech_only_settings.container_settings[form::technology::hdf5]["different_container"] = {};
  CHECK_THROWS_AS(reader.get_index(index_token, "[event:1, segment:1]", tech_only_settings),
                  std::runtime_error);

  std::string const file_name =
    "storage_reader_getindex_attr_" + form::technology::to_string(technology) + ".root";
  std::string const creator = "storage_reader_getindex_attr_creator";
  item_config cfg;
  cfg.add_item("prod", file_name, technology);
  std::vector<int> payload = {5, 6, 7};
  {
    auto writer = create_persistence_writer();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configure_tech_settings(tech_setting_config{});
    writer->create_containers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->register_write(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commit_output(creator, "[event:5, segment:6]");
  }

  tech_setting_config attr_settings;
  attr_settings.file_settings[technology][file_name] = {{"compression", "1"}};
  CHECK(reader.get_index(
          token{file_name, creator + "/index", technology}, "missing-id", attr_settings) == 0);

  tech_setting_config container_attr_settings;
  container_attr_settings.container_settings[technology][creator + "/index"] = {{"split", "0"}};
  CHECK_NOTHROW(reader.get_index(
    token{file_name, creator + "/index", technology}, "missing-id", container_attr_settings));
}

TEST_CASE("storage_reader prime/list_indices/read_container: attribute and error branches",
          "[form]")
{
  using namespace form::experimental::config;

  std::string const file_name =
    "storage_reader_misc_attr_" + form::technology::to_string(technology) + ".root";
  std::string const creator = "storage_reader_misc_creator";
  item_config cfg;
  cfg.add_item("prod", file_name, technology);
  std::vector<int> payload = {9, 8, 7};
  {
    auto writer = create_persistence_writer();
    REQUIRE(writer != nullptr);
    writer->configure(cfg);
    writer->configure_tech_settings(tech_setting_config{});
    writer->create_containers(creator, {{"prod", &typeid(std::vector<int>)}});
    writer->register_write(creator, "prod", &payload, typeid(std::vector<int>));
    writer->commit_output(creator, "[event:9, segment:8]");
  }

  storage_reader reader;
  token const index_token{file_name, creator + "/index", technology};

  tech_setting_config file_attr_settings;
  file_attr_settings.file_settings[technology][file_name] = {{"compression", "1"}};

  CHECK_NOTHROW(reader.prime(index_token, typeid(std::string), file_attr_settings));

  void const* raw = nullptr;
  CHECK_NOTHROW(reader.read_container(token{file_name, creator + "/prod", technology, 0},
                                      &raw,
                                      typeid(std::vector<int>),
                                      file_attr_settings));

  tech_setting_config empty_settings;
  CHECK_THROWS_AS(reader.list_indices(
                    token{"storage_reader_hdf5_misc.root", "creator/index", form::technology::hdf5},
                    empty_settings),
                  std::runtime_error);

  tech_setting_config container_attr_settings;
  container_attr_settings.container_settings[technology][creator + "/index"] = {{"split", "0"}};

  CHECK_NOTHROW(reader.list_indices(index_token, container_attr_settings));
  CHECK_NOTHROW(reader.read_container(token{file_name, creator + "/prod", technology, 0},
                                      &raw,
                                      typeid(std::vector<int>),
                                      container_attr_settings));
}

TEST_CASE("Root branch prime: error paths", "[form]")
{
  SECTION("no file attached throws")
  {
    auto container = create_read_container(technology, "SomeTree/branch");
    CHECK_THROWS_AS(container->prime(typeid(std::vector<int>)), std::runtime_error);
  }

  SECTION("container name not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = create_file(technology, form::test::test_file_name, 'i');
    auto container = create_read_container(technology, "NonExistentTreeForPrime/branch");
    container->set_file(file);
    CHECK_THROWS_AS(container->prime(typeid(std::vector<int>)), std::runtime_error);
  }

  SECTION("branch not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = create_file(technology, form::test::test_file_name, 'i');
    auto container = create_read_container(
      technology, std::string(form::test::test_tree_name) + "/NonExistentBranchForPrime");
    container->set_file(file);
    CHECK_THROWS_AS(container->prime(typeid(std::vector<int>)), std::runtime_error);
  }

  SECTION("unsupported type throws")
  {
    struct local_prime_type {};
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = create_file(technology, form::test::test_file_name, 'i');
    auto container =
      create_read_container(technology, form::test::make_test_branch_name<std::vector<int>>());
    container->set_file(file);
    CHECK_THROWS_AS(container->prime(typeid(local_prime_type)), std::runtime_error);
  }
}

TEST_CASE("Root branch entries: success and error paths", "[form]")
{
  SECTION("no file attached throws")
  {
    auto container = create_read_container(technology, "SomeTree/branch");
    CHECK_THROWS_AS(container->entries(), std::runtime_error);
  }

  SECTION("tree not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = create_file(technology, form::test::test_file_name, 'i');
    auto container = create_read_container(technology, "NonExistentTreeForEntries/branch");
    container->set_file(file);
    CHECK_THROWS_AS(container->entries(), std::runtime_error);
  }

  SECTION("branch not found throws")
  {
    std::vector<int> data = {1};
    form::test::write(technology, data);
    auto file = create_file(technology, form::test::test_file_name, 'i');
    auto container = create_read_container(
      technology, std::string(form::test::test_tree_name) + "/NonExistentBranchForEntries");
    container->set_file(file);
    CHECK_THROWS_AS(container->entries(), std::runtime_error);
  }

  SECTION("valid container returns entry count")
  {
    std::vector<int> data = {10, 20, 30};
    form::test::write(technology, data);
    auto file = create_file(technology, form::test::test_file_name, 'i');
    auto container =
      create_read_container(technology, form::test::make_test_branch_name<std::vector<int>>());
    container->set_file(file);
    CHECK(container->entries() == 1);
  }
}
