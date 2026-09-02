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
#ifdef USE_ROOT_STORAGE
#include "root_storage/root_tbranch_read_container.hpp"
#include "root_storage/root_tbranch_write_container.hpp"
#include "root_storage/root_ttree_write_container.hpp"
#endif
#ifdef USE_RNTUPLE_STORAGE
#include "root_storage/root_rfield_read_container.hpp"
#include "root_storage/root_rfield_write_container.hpp"
#include "root_storage/root_rntuple_write_container.hpp"
#endif
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

using namespace form::detail::experimental;

namespace {
  // Minimal i_persistence_writer that records how it was called, so FORM's "parse once / create
  // once / skip unconfigured" behavior can be checked without a storage backend.
  class spy_persistence_writer : public i_persistence_writer {
  public:
    int create_calls = 0;
    std::vector<std::string> created_containers;
    std::vector<std::string> written_containers;
    int commit_calls = 0;

    void configure_tech_settings(
      form::experimental::config::tech_setting_config const& /*settings*/) override
    {
    }

    void create_containers(
      std::vector<std::pair<placement, std::type_info const*>> const& containers) override
    {
      ++create_calls;
      for (auto const& [plcmnt, type] : containers) {
        created_containers.push_back(plcmnt.container_name());
      }
    }

    token register_write(placement const& plcmnt,
                         void const* /*data*/,
                         std::type_info const& /*type*/) override
    {
      written_containers.push_back(plcmnt.container_name());
      return token{plcmnt.file_name(), plcmnt.container_name(), plcmnt.technology(), 0};
    }

    void commit_place(placement const& /*plcmnt*/, std::string const& /*id*/) override
    {
      ++commit_calls;
    }
  };
}

TEST_CASE("token default constructor", "[form]")
{
  token t;
  CHECK(t.file_name().empty());
  CHECK(t.container_name().empty());
  CHECK(t.technology() == form::technology::id{});
  // Default-constructed token has no id set
  CHECK_FALSE(t.has_id());
}

TEST_CASE("token basics", "[form]")
{
  token t("file.root", "container", form::technology::root_ttree, 42);
  CHECK(t.file_name() == "file.root");
  CHECK(t.container_name() == "container");
  CHECK(t.technology() == form::technology::root_ttree);
  CHECK(t.has_id());
  CHECK(t.id() == 42u);
}

TEST_CASE("technology::id string conversions", "[form]")
{
  using namespace form::technology;

  // Round-trip the implemented backends through from_string / to_string
  CHECK(from_string("ROOT_TTREE") == root_ttree);
  CHECK(from_string("ROOT_RNTUPLE") == root_rntuple);

  CHECK(to_string(root_ttree) == "ROOT_TTREE");
  CHECK(to_string(root_rntuple) == "ROOT_RNTUPLE");
  CHECK(to_string(hdf5) == "HDF5"); // reserved: still names itself for diagnostics

  // HDF5 is reserved but unimplemented: reject it at parse time rather than
  // silently falling back to a different storage.
  CHECK_THROWS_AS(from_string("HDF5"), std::runtime_error);

  // An unknown name throws; an unknown id stringifies to the sentinel
  CHECK_THROWS_AS(from_string("NOT_A_TECH"), std::runtime_error);
  CHECK(to_string(id{}) == "UNKNOWN");
}

TEST_CASE("technology::id members and ordering", "[form]")
{
  using namespace form::technology;

  // (major, minor) decomposition
  CHECK(root_ttree.major == major::root);
  CHECK(root_ttree.minor == 1);
  CHECK(root_rntuple.major == major::root);
  CHECK(root_rntuple.minor == 2);
  CHECK(hdf5.major == major::hdf5);
  CHECK(id{}.major == major::generic);

  // operator<=> compares BOTH parts: same major, different minor stay distinct
  CHECK(root_ttree != root_rntuple);
  CHECK(root_ttree < root_rntuple);
  CHECK(id{} == id{major::generic, 0});
}

TEST_CASE("storage_file basics", "[form]")
{
  storage_file f("test.root", 'o');
  CHECK(f.name() == "test.root");
  CHECK(f.mode() == 'o');
  CHECK_THROWS_AS(f.set_attribute("key", "value"), std::runtime_error);
}

TEST_CASE("storage_read_container basics", "[form]")
{
  storage_read_container c("my_container");
  CHECK(c.name() == "my_container");

  auto f = std::make_shared<storage_file>("test.root", 'o');
  c.set_file(f);

  void const* data = nullptr;
  CHECK_FALSE(c.read(1, &data, typeid(int)));
  c.prime(typeid(int));
  CHECK(c.entries() == 0);

  CHECK_THROWS_AS(c.set_attribute("key", "value"), std::runtime_error);

  SECTION("With slash")
  {
    storage_read_container c("parent/child");
    CHECK(c.top_name() == "parent");
    CHECK(c.col_name() == "child");
  }
  SECTION("Without slash")
  {
    storage_read_container c("no_slash");
    CHECK(c.top_name() == "no_slash");
    CHECK(c.col_name() == "Main");
  }
}

TEST_CASE("storage_write_container basics", "[form]")
{
  storage_write_container c("my_container");
  CHECK(c.name() == "my_container");

  auto f = std::make_shared<storage_file>("test.root", 'o');
  c.set_file(f);

  c.setup_write(typeid(int));
  int value = 0;
  c.fill(&value);
  c.commit();

  CHECK_THROWS_AS(c.set_attribute("key", "value"), std::runtime_error);
}

TEST_CASE("storage_write_association basics", "[form]")
{
  storage_write_association a("my_assoc/extra");
  CHECK(a.name() == "my_assoc"); // maybe_remove_suffix should remove /extra

  a.set_attribute("key",
                  "value"); // storage_write_association overrides set_attribute to do nothing
}

TEST_CASE("storage_associative_write_container basics", "[form]")
{
  SECTION("With slash")
  {
    storage_associative_write_container c("parent/child");
    CHECK(c.top_name() == "parent");
    CHECK(c.col_name() == "child");
  }
  SECTION("Without slash")
  {
    storage_associative_write_container c("no_slash");
    CHECK(c.top_name() == "no_slash");
    CHECK(c.col_name() == "Main");
  }

  storage_associative_write_container c("p/c");
  auto parent = std::make_shared<storage_write_container>("p");
  c.set_parent(parent);
}

TEST_CASE("Factories fallback", "[form]")
{
  auto f = create_file(form::technology::id{}, "test.root", 'o');
  CHECK(dynamic_cast<storage_file*>(f.get()) != nullptr);

  auto rc = create_read_container(form::technology::id{}, "cont");
  CHECK(dynamic_cast<storage_read_container*>(rc.get()) != nullptr);

  auto wa = create_write_association(form::technology::id{}, "assoc");
  CHECK(dynamic_cast<storage_write_association*>(wa.get()) != nullptr);

  auto wc = create_write_container(form::technology::id{}, "cont");
  CHECK(dynamic_cast<storage_write_container*>(wc.get()) != nullptr);

  // HDF5 is reserved but unimplemented: every factory must fail loudly on the
  // hdf5 dispatch branch rather than silently return generic storage.
  CHECK_THROWS_AS(create_file(form::technology::hdf5, "test.h5", 'o'), std::runtime_error);
  CHECK_THROWS_AS(create_read_container(form::technology::hdf5, "cont"), std::runtime_error);
  CHECK_THROWS_AS(create_write_association(form::technology::hdf5, "assoc"), std::runtime_error);
  CHECK_THROWS_AS(create_write_container(form::technology::hdf5, "cont"), std::runtime_error);

  // A major FORM doesn't recognize at all must also fail loudly
  // major has a fixed underlying type, so an out-of-range value is legal at runtime
  auto const unknown_major =
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    form::technology::id{.major = static_cast<form::technology::major>(99), .minor = 0};
  CHECK_THROWS_AS(create_file(unknown_major, "test.dat", 'o'), std::runtime_error);
  CHECK_THROWS_AS(create_read_container(unknown_major, "cont"), std::runtime_error);
  CHECK_THROWS_AS(create_write_association(unknown_major, "assoc"), std::runtime_error);
  CHECK_THROWS_AS(create_write_container(unknown_major, "cont"), std::runtime_error);
}

TEST_CASE("Factories ROOT storage dispatch", "[form]")
{
#ifdef USE_ROOT_STORAGE
  auto rc_ttree = create_read_container(form::technology::root_ttree, "cont");
  CHECK(dynamic_cast<root_tbranch_read_container_imp*>(rc_ttree.get()) != nullptr);

  auto wa_ttree = create_write_association(form::technology::root_ttree, "assoc");
  CHECK(dynamic_cast<root_ttree_write_container_imp*>(wa_ttree.get()) != nullptr);

  auto wc_ttree = create_write_container(form::technology::root_ttree, "cont");
  CHECK(dynamic_cast<root_tbranch_write_container_imp*>(wc_ttree.get()) != nullptr);

  auto const unsupported_root =
    form::technology::id{.major = form::technology::major::root, .minor = 99};
  CHECK_THROWS_AS(create_read_container(unsupported_root, "cont"), std::runtime_error);
  CHECK_THROWS_AS(create_write_association(unsupported_root, "assoc"), std::runtime_error);
  CHECK_THROWS_AS(create_write_container(unsupported_root, "cont"), std::runtime_error);
#else
  CHECK_THROWS_AS(create_read_container(form::technology::root_ttree, "cont"), std::runtime_error);
  CHECK_THROWS_AS(create_write_association(form::technology::root_ttree, "assoc"),
                  std::runtime_error);
  CHECK_THROWS_AS(create_write_container(form::technology::root_ttree, "cont"), std::runtime_error);
#endif
}

TEST_CASE("Factories RNTuple storage dispatch", "[form]")
{
#ifdef USE_RNTUPLE_STORAGE
  auto rc_rntuple = create_read_container(form::technology::root_rntuple, "cont");
  CHECK(dynamic_cast<root_rfield_read_container_imp*>(rc_rntuple.get()) != nullptr);

  auto wa_rntuple = create_write_association(form::technology::root_rntuple, "assoc");
  CHECK(dynamic_cast<root_rntuple_write_container_imp*>(wa_rntuple.get()) != nullptr);

  auto wc_rntuple = create_write_container(form::technology::root_rntuple, "cont");
  CHECK(dynamic_cast<root_rfield_write_container_imp*>(wc_rntuple.get()) != nullptr);
#else
  CHECK_THROWS_AS(create_read_container(form::technology::root_rntuple, "cont"),
                  std::runtime_error);
  CHECK_THROWS_AS(create_write_association(form::technology::root_rntuple, "assoc"),
                  std::runtime_error);
  CHECK_THROWS_AS(create_write_container(form::technology::root_rntuple, "cont"),
                  std::runtime_error);
#endif
}

TEST_CASE("storage_reader basic operations", "[form]")
{
  auto storage = create_storage_reader();
  REQUIRE(storage != nullptr);

  form::experimental::config::tech_setting_config settings;

  token product_token("file.root", "cont", form::technology::id{}, 1);
  void const* read_data = nullptr;
  storage->read_container(product_token, &read_data, typeid(int), settings);

  int index = storage->get_index(product_token, "some_id", settings);
  CHECK(index == 0);
}

TEST_CASE("storage_writer basic operations", "[form]")
{
  auto storage = create_storage_writer();
  REQUIRE(storage != nullptr);

  form::experimental::config::tech_setting_config settings;

  std::map<std::unique_ptr<placement>, std::type_info const*> containers;
  auto p = std::make_unique<placement>("file.root", "cont", form::technology::id{});
  containers.emplace(std::move(p), &typeid(int));

  storage->create_containers(containers, settings);

  placement p2("file.root", "cont", form::technology::id{});
  int data = 42;
  storage->fill_container(p2, &data, typeid(int));
  storage->commit_containers(p2);
}

TEST_CASE("persistence_reader basic operations", "[form]")
{
  auto p = create_persistence_reader();
  REQUIRE(p != nullptr);

  using namespace form::experimental::config;
  item_config out_cfg;
  out_cfg.add_item("prod", "file.root", form::technology::id{});
  out_cfg.add_item("parent/child", "file.root", form::technology::id{});
  p->configure(out_cfg);

  tech_setting_config tech_cfg;
  p->configure_tech_settings(tech_cfg);

  SECTION("Full Lifecycle")
  {
    void const* data = nullptr;
    // This will call get_token -> get_index (returns 0 for Storage_Container) -> read_container
    CHECK_NOTHROW(p->read("my_creator", "prod", "event_1", &data, typeid(int)));
  }
}

TEST_CASE("persistence_writer: register_write rejects a non-row-addressed backend", "[form]")
{
  using namespace form::experimental::config;

  auto p = create_persistence_writer();
  REQUIRE(p != nullptr);
  p->configure_tech_settings(tech_setting_config{});

  // The generic backend's write container is a no-op whose fill() returns invalid_row_id, so the
  // resulting token could never locate the product on read: register_write must reject it rather
  // than return an unusable token.
  placement const generic{"pw_basics_notset.generic", "my_creator/prod", form::technology::id{}};
  p->create_containers({{generic, &typeid(int)}});

  int val = 42;
  CHECK_THROWS_AS(p->register_write(generic, &val, typeid(int)), std::runtime_error);
}

TEST_CASE("form::experimental::config tests", "[form]")
{
  using namespace form::experimental::config;

  SECTION("item_config")
  {
    item_config cfg;
    cfg.add_item("prod1", "file1.root", form::technology::root_ttree);

    auto item = cfg.find_item("prod1");
    REQUIRE(item);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- `REQUIRE` protects against incorrect access
    CHECK(item->product_name == "prod1");

    CHECK_FALSE(cfg.find_item("nonexistent").has_value());
  }

  SECTION("tech_setting_config")
  {
    tech_setting_config cfg;
    cfg.file_settings[form::technology::root_ttree]["file1.root"] = {{"attr", "val"}};
    cfg.container_settings[form::technology::root_ttree]["cont1"] = {{"cattr", "cval"}};

    auto ftable = cfg.get_file_table(form::technology::root_ttree, "file1.root");
    REQUIRE(ftable.size() == 1);
    CHECK(ftable[0].first == "attr");
    CHECK(ftable[0].second == "val");

    auto ctable = cfg.get_container_table(form::technology::root_ttree, "cont1");
    REQUIRE(ctable.size() == 1);
    CHECK(ctable[0].first == "cattr");
    CHECK(ctable[0].second == "cval");
  }
}

TEST_CASE("FORM source registry prefers exact type matches", "[form]")
{
  struct local_product {
    int value{};
  };

  constexpr char const* local_name = "std::vector<local_product>";

  form::experimental::register_form_vector_product_type<local_product>(local_name);

  auto const local_type = phlex::detail::make_type_id<std::vector<local_product>>();
  auto const* resolved_name = form::experimental::find_form_product_type_name(local_type);

  REQUIRE(resolved_name != nullptr);
  CHECK(*resolved_name == local_name);

  auto const* entry = form::experimental::find_form_product_type(*resolved_name);
  REQUIRE(entry != nullptr);
  REQUIRE(entry->cpp_type != nullptr);
  CHECK(*entry->cpp_type == typeid(std::vector<local_product>));
}

TEST_CASE("FORM source registry keeps builtin mappings", "[form]")
{
  auto const bool_type = phlex::detail::make_type_id<std::vector<bool>>();
  auto const* resolved_name = form::experimental::find_form_product_type_name(bool_type);

  REQUIRE(resolved_name != nullptr);
  CHECK(*resolved_name == "std::vector<bool>");
}

TEST_CASE("persistence_reader: throws for missing product in config", "[form]")
{
  using namespace form::experimental::config;

  auto reader = form::detail::experimental::create_persistence_reader();
  REQUIRE(reader != nullptr);
  reader->configure(item_config{});
  reader->configure_tech_settings(tech_setting_config{});

  CHECK_THROWS_AS(reader->prime("creator", "nonexistent", typeid(int)), std::runtime_error);
  CHECK_THROWS_AS(reader->list_indices("creator", "nonexistent"), std::runtime_error);
}

TEST_CASE("form_reader_interface::indices exercises persistence list_indices path", "[form]")
{
  using namespace form::experimental::config;

  item_config cfg;
  cfg.add_item("prod", "dummy_reader_test.root", form::technology::id{});
  form::experimental::form_reader_interface reader{cfg, tech_setting_config{}};

  // indices() calls persistence list_indices; with tech=0 the index container is
  // always empty, so it throws -- but the call itself covers form_reader.cpp L48.
  CHECK_THROWS_AS(reader.indices("creator", "prod"), std::runtime_error);
}

TEST_CASE("form_reader_interface::read throws for missing product config", "[form]")
{
  using namespace form::experimental::config;

  item_config cfg;
  cfg.add_item("prod", "dummy_reader_test.root", form::technology::id{});
  form::experimental::form_reader_interface reader{cfg, tech_setting_config{}};

  form::experimental::product_with_name product{
    .label = "missing", .data = nullptr, .type = &typeid(int)};
  CHECK_THROWS_AS(reader.read("creator", "segment", product), std::runtime_error);
}

TEST_CASE("form_writer_interface handles missing product config without crashing", "[form]")
{
  using namespace form::experimental::config;

  item_config cfg;
  cfg.add_item("prod", "dummy_writer_test.root", form::technology::id{});
  form::experimental::form_writer_interface writer{cfg, tech_setting_config{}};

  form::experimental::product_with_name product{
    .label = "missing", .data = nullptr, .type = &typeid(int)};
  CHECK_NOTHROW(writer.write("creator", "segment", product));
}

TEST_CASE("form_writer_interface creates containers once across events", "[form]")
{
  using namespace form::experimental::config;

  item_config cfg;
  cfg.add_item("prod", "form_writer_create_once.root", form::technology::root_ttree);

  auto spy = std::make_unique<spy_persistence_writer>();
  auto* spy_raw = spy.get();
  form::experimental::form_writer_interface writer{cfg, tech_setting_config{}, std::move(spy)};

  int payload = 7;
  form::experimental::product_with_name product{
    .label = "prod", .data = &payload, .type = &typeid(int)};

  writer.write("creator", "[event:1]", std::vector{product});
  writer.write("creator", "[event:2]", std::vector{product});

  // Containers are created on the first event only; writes and commits still happen every event.
  CHECK(spy_raw->create_calls == 1);
  CHECK(spy_raw->written_containers.size() == 2);
  CHECK(spy_raw->commit_calls == 2);
}

TEST_CASE("form_writer_interface fans a product out to multiple destinations", "[form]")
{
  using namespace form::experimental::config;

  // The same product is configured for two destinations.
  item_config cfg;
  cfg.add_item("prod", "form_writer_fanout_a.root", form::technology::root_ttree);
  cfg.add_item("prod", "form_writer_fanout_b.root", form::technology::root_ttree);

  auto spy = std::make_unique<spy_persistence_writer>();
  auto* spy_raw = spy.get();
  form::experimental::form_writer_interface writer{cfg, tech_setting_config{}, std::move(spy)};

  int payload = 7;
  form::experimental::product_with_name product{
    .label = "prod", .data = &payload, .type = &typeid(int)};

  writer.write("creator", "[event:1]", std::vector{product});
  writer.write("creator", "[event:2]", std::vector{product});

  // FORM names only the two product placements (the index is persistence's concern now), created
  // once on the first event.
  CHECK(spy_raw->create_calls == 1);
  CHECK(spy_raw->created_containers.size() == 2);
  // The product is filled into both destinations every event (2 places x 2 events)...
  CHECK(spy_raw->written_containers.size() == 4);
  // ...and each place is committed every event (2 places x 2 events): a place's row is only
  // written when that place is committed.
  CHECK(spy_raw->commit_calls == 4);
}

TEST_CASE("form_writer_interface skips unconfigured products in a vector write", "[form]")
{
  using namespace form::experimental::config;

  item_config cfg;
  cfg.add_item("prod", "form_writer_skip.root", form::technology::root_ttree);

  auto spy = std::make_unique<spy_persistence_writer>();
  auto* spy_raw = spy.get();
  form::experimental::form_writer_interface writer{cfg, tech_setting_config{}, std::move(spy)};

  int payload = 7;
  form::experimental::product_with_name unconfigured{
    .label = "missing", .data = &payload, .type = &typeid(int)};

  CHECK_NOTHROW(writer.write("creator", "[event:1]", std::vector{unconfigured}));

  // Nothing is configured for "missing": no container created, nothing written or committed.
  CHECK(spy_raw->create_calls == 0);
  CHECK(spy_raw->written_containers.empty());
  CHECK(spy_raw->commit_calls == 0);
}

TEST_CASE("form_writer_interface rejects a null injected persistence writer", "[form]")
{
  using namespace form::experimental::config;

  item_config cfg;
  cfg.add_item("prod", "form_writer_null_pers.root", form::technology::root_ttree);

  // The injecting constructor must fail loudly if handed a null persistence writer rather than
  // store it and crash on first use.
  CHECK_THROWS_AS((form::experimental::form_writer_interface{
                    cfg, tech_setting_config{}, std::unique_ptr<i_persistence_writer>{}}),
                  std::runtime_error);
}

TEST_CASE("form_writer_interface rejects a product first appearing at a sealed place", "[form]")
{
  using namespace form::experimental::config;

  // Two products share one destination (same file + technology), so they land in the same place.
  item_config cfg;
  cfg.add_item("early", "form_writer_seal.root", form::technology::root_ttree);
  cfg.add_item("late", "form_writer_seal.root", form::technology::root_ttree);

  auto spy = std::make_unique<spy_persistence_writer>();
  form::experimental::form_writer_interface writer{cfg, tech_setting_config{}, std::move(spy)};

  int payload = 7;
  form::experimental::product_with_name early{
    .label = "early", .data = &payload, .type = &typeid(int)};
  form::experimental::product_with_name late{
    .label = "late", .data = &payload, .type = &typeid(int)};

  // Record 1 writes "early", sealing the place's container structure.
  writer.write("creator", "[event:1]", std::vector{early});
  // Record 2 introduces "late" at that already-sealed place: FORM rejects it rather than let the
  // backend crash adding a container after first write.
  CHECK_THROWS_AS(writer.write("creator", "[event:2]", std::vector{late}), std::runtime_error);
}

TEST_CASE("form_writer_interface commits only the places written this record", "[form]")
{
  using namespace form::experimental::config;

  // Two products go to two distinct destinations, so they occupy two separate places.
  item_config cfg;
  cfg.add_item("a", "form_writer_commit_a.root", form::technology::root_ttree);
  cfg.add_item("b", "form_writer_commit_b.root", form::technology::root_ttree);

  auto spy = std::make_unique<spy_persistence_writer>();
  auto* spy_raw = spy.get();
  form::experimental::form_writer_interface writer{cfg, tech_setting_config{}, std::move(spy)};

  int payload = 7;
  form::experimental::product_with_name a{.label = "a", .data = &payload, .type = &typeid(int)};
  form::experimental::product_with_name b{.label = "b", .data = &payload, .type = &typeid(int)};

  // Record 1 writes both products: both places are committed.
  writer.write("creator", "[event:1]", std::vector{a, b});
  // Record 2 writes only "a": "b"'s place is known but received no data, so it is not committed.
  writer.write("creator", "[event:2]", std::vector{a});

  // 2 commits on record 1 (a, b) + 1 commit on record 2 (a only) = 3.
  CHECK(spy_raw->commit_calls == 3);
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
  struct never_registered {};
  auto const unknown_type = phlex::detail::make_type_id<never_registered>();
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
