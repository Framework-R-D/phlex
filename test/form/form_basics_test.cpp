#include "core/technology.hpp"
#include "core/token.hpp"
#include "form/config.hpp"
#include "form/form_reader.hpp"
#include "form/form_source_type_registry.hpp"
#include "form/form_writer.hpp"
#include "persistence/persistence_reader.hpp"
#include "persistence/persistence_writer.hpp"
#include "storage/factories.hpp"
#include "storage/istorage.hpp"
#include "storage/storage_associative_write_container.hpp"
#include "storage/storage_file.hpp"
#include "storage/storage_read_container.hpp"
#include "storage/storage_write_association.hpp"
#include "storage/storage_write_container.hpp"
#if defined(USE_ROOT_STORAGE)
#include "root_storage/root_tbranch_read_container.hpp"
#include "root_storage/root_tbranch_write_container.hpp"
#include "root_storage/root_ttree_write_container.hpp"
#endif
#if defined(USE_RNTUPLE_STORAGE)
#include "root_storage/root_rfield_read_container.hpp"
#include "root_storage/root_rfield_write_container.hpp"
#include "root_storage/root_rntuple_write_container.hpp"
#endif
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <vector>

using namespace form::detail::experimental;

TEST_CASE("Token default constructor", "[form]")
{
  Token t;
  CHECK(t.fileName().empty());
  CHECK(t.containerName().empty());
  CHECK(t.technology() == form::technology::Id{});
  // Default-constructed token must carry the -1 sentinel for id
  CHECK(t.id() == -1);
}

TEST_CASE("Token basics", "[form]")
{
  Token t("file.root", "container", form::technology::ROOT_TTREE, 42);
  CHECK(t.fileName() == "file.root");
  CHECK(t.containerName() == "container");
  CHECK(t.technology() == form::technology::ROOT_TTREE);
  CHECK(t.id() == 42);
}

TEST_CASE("technology::Id string conversions", "[form]")
{
  using namespace form::technology;

  // Round-trip the implemented backends through from_string / to_string
  CHECK(from_string("ROOT_TTREE") == ROOT_TTREE);
  CHECK(from_string("ROOT_RNTUPLE") == ROOT_RNTUPLE);

  CHECK(to_string(ROOT_TTREE) == "ROOT_TTREE");
  CHECK(to_string(ROOT_RNTUPLE) == "ROOT_RNTUPLE");
  CHECK(to_string(HDF5) == "HDF5"); // reserved: still names itself for diagnostics

  // HDF5 is reserved but unimplemented: reject it at parse time rather than
  // silently falling back to a different storage.
  CHECK_THROWS_AS(from_string("HDF5"), std::runtime_error);

  // An unknown name throws; an unknown Id stringifies to the sentinel
  CHECK_THROWS_AS(from_string("NOT_A_TECH"), std::runtime_error);
  CHECK(to_string(Id{}) == "UNKNOWN");
}

TEST_CASE("technology::Id members and ordering", "[form]")
{
  using namespace form::technology;

  // (major, minor) decomposition
  CHECK(ROOT_TTREE.major == Major::root);
  CHECK(ROOT_TTREE.minor == 1);
  CHECK(ROOT_RNTUPLE.major == Major::root);
  CHECK(ROOT_RNTUPLE.minor == 2);
  CHECK(HDF5.major == Major::hdf5);
  CHECK(Id{}.major == Major::unknown);

  // operator<=> compares BOTH parts: same major, different minor stay distinct
  CHECK(ROOT_TTREE != ROOT_RNTUPLE);
  CHECK(ROOT_TTREE < ROOT_RNTUPLE);
  CHECK(Id{} == Id{Major::unknown, 0});
}

TEST_CASE("Storage_File basics", "[form]")
{
  Storage_File f("test.root", 'o');
  CHECK(f.name() == "test.root");
  CHECK(f.mode() == 'o');
  CHECK_THROWS_AS(f.setAttribute("key", "value"), std::runtime_error);
}

TEST_CASE("Storage_Read_Container basics", "[form]")
{
  Storage_Read_Container c("my_container");
  CHECK(c.name() == "my_container");

  auto f = std::make_shared<Storage_File>("test.root", 'o');
  c.setFile(f);

  void const* data = nullptr;
  CHECK_FALSE(c.read(1, &data, typeid(int)));
  c.prime(typeid(int));
  CHECK(c.entries() == 0);

  CHECK_THROWS_AS(c.setAttribute("key", "value"), std::runtime_error);

  SECTION("With slash")
  {
    Storage_Read_Container c("parent/child");
    CHECK(c.top_name() == "parent");
    CHECK(c.col_name() == "child");
  }
  SECTION("Without slash")
  {
    Storage_Read_Container c("no_slash");
    CHECK(c.top_name() == "no_slash");
    CHECK(c.col_name() == "Main");
  }
}

TEST_CASE("Storage_Write_Container basics", "[form]")
{
  Storage_Write_Container c("my_container");
  CHECK(c.name() == "my_container");

  auto f = std::make_shared<Storage_File>("test.root", 'o');
  c.setFile(f);

  c.setupWrite(typeid(int));
  int value = 0;
  c.fill(&value);
  c.commit();

  CHECK_THROWS_AS(c.setAttribute("key", "value"), std::runtime_error);
}

TEST_CASE("Storage_Write_Association basics", "[form]")
{
  Storage_Write_Association a("my_assoc/extra");
  CHECK(a.name() == "my_assoc"); // maybe_remove_suffix should remove /extra

  a.setAttribute("key", "value"); // Storage_Write_Association overrides setAttribute to do nothing
}

TEST_CASE("Storage_Associative_Write_Container basics", "[form]")
{
  SECTION("With slash")
  {
    Storage_Associative_Write_Container c("parent/child");
    CHECK(c.top_name() == "parent");
    CHECK(c.col_name() == "child");
  }
  SECTION("Without slash")
  {
    Storage_Associative_Write_Container c("no_slash");
    CHECK(c.top_name() == "no_slash");
    CHECK(c.col_name() == "Main");
  }

  Storage_Associative_Write_Container c("p/c");
  auto parent = std::make_shared<Storage_Write_Container>("p");
  c.setParent(parent);
}

TEST_CASE("Factories fallback", "[form]")
{
  auto f = createFile(form::technology::Id{}, "test.root", 'o');
  CHECK(dynamic_cast<Storage_File*>(f.get()) != nullptr);

  auto rc = createReadContainer(form::technology::Id{}, "cont");
  CHECK(dynamic_cast<Storage_Read_Container*>(rc.get()) != nullptr);

  auto wa = createWriteAssociation(form::technology::Id{}, "assoc");
  CHECK(dynamic_cast<Storage_Write_Association*>(wa.get()) != nullptr);

  auto wc = createWriteContainer(form::technology::Id{}, "cont");
  CHECK(dynamic_cast<Storage_Write_Container*>(wc.get()) != nullptr);

  // HDF5 is reserved but unimplemented: every factory must fail loudly on the
  // hdf5 dispatch branch rather than silently return generic storage.
  CHECK_THROWS_AS(createFile(form::technology::HDF5, "test.h5", 'o'), std::runtime_error);
  CHECK_THROWS_AS(createReadContainer(form::technology::HDF5, "cont"), std::runtime_error);
  CHECK_THROWS_AS(createWriteAssociation(form::technology::HDF5, "assoc"), std::runtime_error);
  CHECK_THROWS_AS(createWriteContainer(form::technology::HDF5, "cont"), std::runtime_error);

  // A major FORM doesn't recognize at all must also fail loudly
  // Major has a fixed underlying type, so an out-of-range value is legal at runtime
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  auto const unknown_major = form::technology::Id{static_cast<form::technology::Major>(99), 0};
  CHECK_THROWS_AS(createFile(unknown_major, "test.dat", 'o'), std::runtime_error);
  CHECK_THROWS_AS(createReadContainer(unknown_major, "cont"), std::runtime_error);
  CHECK_THROWS_AS(createWriteAssociation(unknown_major, "assoc"), std::runtime_error);
  CHECK_THROWS_AS(createWriteContainer(unknown_major, "cont"), std::runtime_error);
}

TEST_CASE("Factories ROOT storage dispatch", "[form]")
{
#if defined(USE_ROOT_STORAGE)
  auto rc_ttree = createReadContainer(form::technology::ROOT_TTREE, "cont");
  CHECK(dynamic_cast<ROOT_TBranch_Read_ContainerImp*>(rc_ttree.get()) != nullptr);

  auto wa_ttree = createWriteAssociation(form::technology::ROOT_TTREE, "assoc");
  CHECK(dynamic_cast<ROOT_TTree_Write_ContainerImp*>(wa_ttree.get()) != nullptr);

  auto wc_ttree = createWriteContainer(form::technology::ROOT_TTREE, "cont");
  CHECK(dynamic_cast<ROOT_TBranch_Write_ContainerImp*>(wc_ttree.get()) != nullptr);
#else
  CHECK_THROWS_AS(createReadContainer(form::technology::ROOT_TTREE, "cont"), std::runtime_error);
  CHECK_THROWS_AS(createWriteAssociation(form::technology::ROOT_TTREE, "assoc"),
                  std::runtime_error);
  CHECK_THROWS_AS(createWriteContainer(form::technology::ROOT_TTREE, "cont"), std::runtime_error);
#endif
}

TEST_CASE("Factories RNTuple storage dispatch", "[form]")
{
#if defined(USE_RNTUPLE_STORAGE)
  auto rc_rntuple = createReadContainer(form::technology::ROOT_RNTUPLE, "cont");
  CHECK(dynamic_cast<ROOT_RField_Read_ContainerImp*>(rc_rntuple.get()) != nullptr);

  auto wa_rntuple = createWriteAssociation(form::technology::ROOT_RNTUPLE, "assoc");
  CHECK(dynamic_cast<ROOT_RNTuple_Write_ContainerImp*>(wa_rntuple.get()) != nullptr);

  auto wc_rntuple = createWriteContainer(form::technology::ROOT_RNTUPLE, "cont");
  CHECK(dynamic_cast<ROOT_RField_Write_ContainerImp*>(wc_rntuple.get()) != nullptr);
#else
  CHECK_THROWS_AS(createReadContainer(form::technology::ROOT_RNTUPLE, "cont"), std::runtime_error);
  CHECK_THROWS_AS(createWriteAssociation(form::technology::ROOT_RNTUPLE, "assoc"),
                  std::runtime_error);
  CHECK_THROWS_AS(createWriteContainer(form::technology::ROOT_RNTUPLE, "cont"), std::runtime_error);
#endif
}

TEST_CASE("StorageReader basic operations", "[form]")
{
  auto storage = createStorageReader();
  REQUIRE(storage != nullptr);

  form::experimental::config::tech_setting_config settings;

  Token token("file.root", "cont", form::technology::Id{}, 1);
  void const* read_data = nullptr;
  storage->readContainer(token, &read_data, typeid(int), settings);

  int index = storage->getIndex(token, "some_id", settings);
  CHECK(index == 0);
}

TEST_CASE("StorageWriter basic operations", "[form]")
{
  auto storage = createStorageWriter();
  REQUIRE(storage != nullptr);

  form::experimental::config::tech_setting_config settings;

  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  auto p = std::make_unique<Placement>("file.root", "cont", form::technology::Id{});
  containers.emplace(std::move(p), &typeid(int));

  storage->createContainers(containers, settings);

  Placement p2("file.root", "cont", form::technology::Id{});
  int data = 42;
  storage->fillContainer(p2, &data, typeid(int));
  storage->commitContainers(p2);
}

TEST_CASE("PersistenceReader basic operations", "[form]")
{
  auto p = createPersistenceReader();
  REQUIRE(p != nullptr);

  using namespace form::experimental::config;
  ItemConfig out_cfg;
  out_cfg.addItem("prod", "file.root", form::technology::Id{});
  out_cfg.addItem("parent/child", "file.root", form::technology::Id{});
  p->configure(out_cfg);

  tech_setting_config tech_cfg;
  p->configureTechSettings(tech_cfg);

  SECTION("Full Lifecycle")
  {
    void const* data = nullptr;
    // This will call getToken -> getIndex (returns 0 for Storage_Container) -> readContainer
    CHECK_NOTHROW(p->read("my_creator", "prod", "event_1", &data, typeid(int)));
  }
}

TEST_CASE("PersistenceWriter basic operations", "[form]")
{
  auto p = createPersistenceWriter();
  REQUIRE(p != nullptr);

  using namespace form::experimental::config;
  ItemConfig out_cfg;
  out_cfg.addItem("prod", "file.root", form::technology::Id{});
  out_cfg.addItem("parent/child", "file.root", form::technology::Id{});
  p->configure(out_cfg);

  tech_setting_config tech_cfg;
  p->configureTechSettings(tech_cfg);

  SECTION("Errors")
  {
    int val = 42;
    CHECK_THROWS_AS(p->registerWrite("my_creator", "unknown", &val, typeid(int)),
                    std::runtime_error);
  }
}

TEST_CASE("form::experimental::config tests", "[form]")
{
  using namespace form::experimental::config;

  SECTION("ItemConfig")
  {
    ItemConfig cfg;
    cfg.addItem("prod1", "file1.root", form::technology::ROOT_TTREE);

    auto item = cfg.findItem("prod1");
    REQUIRE(item);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- `REQUIRE` protects against incorrect access
    CHECK(item->product_name == "prod1");

    CHECK_FALSE(cfg.findItem("nonexistent").has_value());
  }

  SECTION("tech_setting_config")
  {
    tech_setting_config cfg;
    cfg.file_settings[form::technology::ROOT_TTREE]["file1.root"] = {{"attr", "val"}};
    cfg.container_settings[form::technology::ROOT_TTREE]["cont1"] = {{"cattr", "cval"}};

    auto ftable = cfg.getFileTable(form::technology::ROOT_TTREE, "file1.root");
    REQUIRE(ftable.size() == 1);
    CHECK(ftable[0].first == "attr");
    CHECK(ftable[0].second == "val");

    auto ctable = cfg.getContainerTable(form::technology::ROOT_TTREE, "cont1");
    REQUIRE(ctable.size() == 1);
    CHECK(ctable[0].first == "cattr");
    CHECK(ctable[0].second == "cval");
  }
}

TEST_CASE("FORM source registry prefers exact type matches", "[form]")
{
  struct LocalProduct {
    int value{};
  };

  constexpr char const* local_name = "std::vector<LocalProduct>";

  form::experimental::register_form_vector_product_type<LocalProduct>(local_name);

  auto const local_type = phlex::detail::make_type_id<std::vector<LocalProduct>>();
  auto const* resolved_name = form::experimental::find_form_product_type_name(local_type);

  REQUIRE(resolved_name != nullptr);
  CHECK(*resolved_name == local_name);

  auto const* entry = form::experimental::find_form_product_type(*resolved_name);
  REQUIRE(entry != nullptr);
  REQUIRE(entry->cpp_type != nullptr);
  CHECK(*entry->cpp_type == typeid(std::vector<LocalProduct>));
}

TEST_CASE("FORM source registry keeps builtin mappings", "[form]")
{
  auto const bool_type = phlex::detail::make_type_id<std::vector<bool>>();
  auto const* resolved_name = form::experimental::find_form_product_type_name(bool_type);

  REQUIRE(resolved_name != nullptr);
  CHECK(*resolved_name == "std::vector<bool>");
}

TEST_CASE("PersistenceReader: throws for missing product in config", "[form]")
{
  using namespace form::experimental::config;

  auto reader = form::detail::experimental::createPersistenceReader();
  REQUIRE(reader != nullptr);
  reader->configure(ItemConfig{});
  reader->configureTechSettings(tech_setting_config{});

  CHECK_THROWS_AS(reader->prime("creator", "nonexistent", typeid(int)), std::runtime_error);
  CHECK_THROWS_AS(reader->listIndices("creator", "nonexistent"), std::runtime_error);
}

TEST_CASE("form_reader_interface::indices exercises persistence listIndices path", "[form]")
{
  using namespace form::experimental::config;

  ItemConfig cfg;
  cfg.addItem("prod", "dummy_reader_test.root", form::technology::Id{});
  form::experimental::form_reader_interface reader{cfg, tech_setting_config{}};

  // indices() calls persistence listIndices; with tech=0 the index container is
  // always empty, so it throws -- but the call itself covers form_reader.cpp L48.
  CHECK_THROWS_AS(reader.indices("creator", "prod"), std::runtime_error);
}

TEST_CASE("form_reader_interface::read throws for missing product config", "[form]")
{
  using namespace form::experimental::config;

  ItemConfig cfg;
  cfg.addItem("prod", "dummy_reader_test.root", form::technology::Id{});
  form::experimental::form_reader_interface reader{cfg, tech_setting_config{}};

  form::experimental::product_with_name product{"missing", nullptr, &typeid(int)};
  CHECK_THROWS_AS(reader.read("creator", "segment", product), std::runtime_error);
}

TEST_CASE("form_writer_interface handles missing product config without crashing", "[form]")
{
  using namespace form::experimental::config;

  ItemConfig cfg;
  cfg.addItem("prod", "dummy_writer_test.root", form::technology::Id{});
  form::experimental::form_writer_interface writer{cfg, tech_setting_config{}};

  form::experimental::product_with_name product{"missing", nullptr, &typeid(int)};
  CHECK_NOTHROW(writer.write("creator", "segment", product));
}

TEST_CASE("form_source_type_registry product_from_data_fn throws on null data", "[form]")
{
  using namespace form::experimental;

  ensure_builtin_form_product_types_registered();
  auto const* entry = find_form_product_type("std::vector<int>");
  REQUIRE(entry != nullptr);
  REQUIRE(entry->product_from_data_fn != nullptr);

  CHECK_THROWS_AS(entry->product_from_data_fn(nullptr, "prod", "[]"), std::runtime_error);
}

TEST_CASE("FORM source registry: unregistered type returns nullptr", "[form]")
{
  // find_form_product_type_name returns nullptr for a type never registered.
  // Exercises the null-return path form_source::create_providers checks at L76-77.
  struct NeverRegistered {};
  auto const unknown_type = phlex::detail::make_type_id<NeverRegistered>();
  CHECK(form::experimental::find_form_product_type_name(unknown_type) == nullptr);
}

TEST_CASE("FORM source registry: unknown name returns nullptr entry", "[form]")
{
  // find_form_product_type returns nullptr for an unregistered name.
  // Exercises the null-entry path form_source::create_providers checks at L80-82.
  CHECK(form::experimental::find_form_product_type("__nonexistent_product_type__") == nullptr);
}

TEST_CASE("FORM source registry: registration error paths", "[form]")
{
  using phlex::detail::make_type_id;

  SECTION("empty product type name throws")
  {
    CHECK_THROWS_AS(
      form::experimental::register_form_product_type(
        "",
        make_type_id<int>(),
        typeid(int),
        [](void const*, std::string const&, std::string const&) -> phlex::detail::product_ptr {
          return nullptr;
        }),
      std::runtime_error);
  }

  SECTION("null conversion function throws")
  {
    CHECK_THROWS_AS(form::experimental::register_form_product_type(
                      "some_new_type_for_error_test",
                      make_type_id<double>(),
                      typeid(double),
                      form::experimental::form_source_product_from_data_fn{}),
                    std::runtime_error);
  }
}
