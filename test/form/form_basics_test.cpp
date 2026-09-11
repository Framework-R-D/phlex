#include "core/cell_index.hpp"
#include "core/container_naming.hpp"
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

#include <algorithm>
#include <cstdint>
#include <map>
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

    void commit_place(placement const& /*plcmnt*/, cell_index const& /*cell*/) override
    {
      ++commit_calls;
    }

    void finalize() override {}
  };

  // Minimal storage backend used to inspect the navigation tables without ROOT.
  class spy_storage_writer : public i_storage_writer {
  public:
    struct table {
      std::vector<std::string> columns;
      std::vector<std::vector<std::string>> rows;
    };

    std::map<std::string, table> tables;
    std::vector<std::string> filled_containers;
    std::string throw_on_fill;

    void create_containers(
      std::map<std::unique_ptr<placement>, std::type_info const*> const& containers,
      form::experimental::config::tech_setting_config const& /*settings*/) override
    {
      for (auto const& [plcmnt, type] : containers) {
        auto const [top, column] = split(plcmnt->container_name());
        if (top.starts_with("nav_")) {
          tables[top].columns.push_back(column);
        }
      }
    }

    std::uint64_t fill_container(placement const& plcmnt,
                                 void const* data,
                                 std::type_info const& type) override
    {
      if (!throw_on_fill.empty() && plcmnt.container_name() == throw_on_fill) {
        throw std::runtime_error("spy_storage_writer: forced failure on " + throw_on_fill);
      }
      filled_containers.push_back(plcmnt.container_name());

      auto const [top, column] = split(plcmnt.container_name());
      if (top.starts_with("nav_")) {
        // Values are read at commit time.
        bound_[top].push_back(bound_value{.data = data, .type = &type});
      }
      // Each container has its own row counter.
      return rows_[plcmnt.container_name()]++;
    }

    void commit_containers(placement const& plcmnt) override
    {
      auto const [top, column] = split(plcmnt.container_name());
      auto const it = bound_.find(top);
      if (it == bound_.end()) {
        return;
      }
      std::vector<std::string> row;
      row.reserve(it->second.size());
      for (auto const& value : it->second) {
        row.push_back(*value.type == typeid(std::string)
                        ? *static_cast<std::string const*>(value.data)
                        : std::to_string(*static_cast<std::uint64_t const*>(value.data)));
      }
      tables[top].rows.push_back(std::move(row));
      it->second.clear();
    }

    /// Force the next row for a container.
    void set_next_row(std::string const& container_name, std::uint64_t row)
    {
      rows_[container_name] = row;
    }

  private:
    /// Value bound until commit.
    struct bound_value {
      void const* data;
      std::type_info const* type;
    };

    static std::pair<std::string, std::string> split(std::string const& container_name)
    {
      auto const slash = container_name.find('/');
      if (slash == std::string::npos) {
        return {container_name, std::string{}};
      }
      return {container_name.substr(0, slash), container_name.substr(slash + 1)};
    }

    std::map<std::string, std::uint64_t> rows_;
    std::map<std::string, std::vector<bound_value>> bound_;
  };

  std::string num(std::uint64_t value) { return std::to_string(value); }

  /// Single-layer data cell.
  cell_index event_cell(std::uint64_t event)
  {
    return cell_index{.id = "[event:" + std::to_string(event) + "]",
                      .layer_names = {"event"},
                      .layer_values = {event}};
  }

  /// Two-layer data cell.
  cell_index event_segment_cell(std::uint64_t event, std::uint64_t segment)
  {
    return cell_index{.id = "[event:" + std::to_string(event) +
                            ", segment:" + std::to_string(segment) + "]",
                      .layer_names = {"event", "segment"},
                      .layer_values = {event, segment}};
  }

  placement product_place(std::string const& creator,
                          std::string const& label,
                          form::technology::id tech = form::technology::id{})
  {
    return placement{"nav_test.root", build_full_label(creator, label), tech};
  }
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
  CHECK_NOTHROW(writer.write("creator", event_cell(1), product));
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

  writer.write("creator", event_cell(1), std::vector{product});
  writer.write("creator", event_cell(2), std::vector{product});

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

  writer.write("creator", event_cell(1), std::vector{product});
  writer.write("creator", event_cell(2), std::vector{product});

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

  CHECK_NOTHROW(writer.write("creator", event_cell(1), std::vector{unconfigured}));

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
  writer.write("creator", event_cell(1), std::vector{early});
  // Record 2 introduces "late" at that already-sealed place: FORM rejects it rather than let the
  // backend crash adding a container after first write.
  CHECK_THROWS_AS(writer.write("creator", event_cell(2), std::vector{late}), std::runtime_error);
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
  writer.write("creator", event_cell(1), std::vector{a, b});
  // Record 2 writes only "a": "b"'s place is known but received no data, so it is not committed.
  writer.write("creator", event_cell(2), std::vector{a});

  // 2 commits on record 1 (a, b) + 1 commit on record 2 (a only) = 3.
  CHECK(spy_raw->commit_calls == 3);
}

namespace {
  class failing_finalize_writer : public spy_persistence_writer {
  public:
    bool throw_std_exception = true;

    void finalize() override
    {
      if (throw_std_exception) {
        throw std::runtime_error("finalize failed");
      }
      throw 42; // not derived from std::exception
    }
  };
}

TEST_CASE("form_writer_interface destruction survives a failing finalize", "[form]")
{
  using namespace form::experimental::config;

  item_config cfg;
  cfg.add_item("prod", "form_writer_finalize_fails.root", form::technology::root_ttree);

  SECTION("a std::exception is reported, not propagated")
  {
    auto spy = std::make_unique<failing_finalize_writer>();
    CHECK_NOTHROW([&] {
      form::experimental::form_writer_interface writer{cfg, tech_setting_config{}, std::move(spy)};
    }());
  }

  SECTION("an unknown exception is reported, not propagated")
  {
    auto spy = std::make_unique<failing_finalize_writer>();
    spy->throw_std_exception = false;
    CHECK_NOTHROW([&] {
      form::experimental::form_writer_interface writer{cfg, tech_setting_config{}, std::move(spy)};
    }());
  }
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

// -------------------------------------------------------------------------------------------------
// Navigation layout: one navigation container per hierarchy per backend, plus a product dictionary.
// -------------------------------------------------------------------------------------------------

TEST_CASE("hierarchy keys and navigation column names", "[form]")
{
  SECTION("a hierarchy is its layer names, as the data cell carried them")
  {
    CHECK(hierarchy_key({"run", "subrun", "event"}) == "run_subrun_event");
    CHECK(hierarchy_key({"event", "segment"}) == "event_segment");
  }

  SECTION("the job cell has no layers")
  {
    CHECK(hierarchy_key({}) == "job");
    CHECK(cell_index{.id = "[]"}.is_job());
  }

  SECTION("a layer the framework left unnamed still gets a usable column")
  {
    CHECK(unnamed_layer_name(0) == "layer0");
    CHECK(hierarchy_key({unnamed_layer_name(0)}) == "layer0");
  }

  SECTION("names ROOT rejects are sanitized")
  {
    CHECK(sanitize_name("plugin:algorithm") == "plugin_algorithm");
    CHECK(sanitize_name("a.b") == "a_b");
    CHECK(navigation_row_column("plugin:algorithm") == "plugin_algorithm_row");
    CHECK(hierarchy_key({"a.b", "c"}) == "a_b_c");
  }

  SECTION("parallel layer vectors are what makes a cell usable")
  {
    CHECK(
      cell_index{.id = "[event:1]", .layer_names = {"event"}, .layer_values = {1}}.consistent());
    CHECK_FALSE(cell_index{.id = "[event:1]", .layer_names = {"event"}}.consistent());
  }

  SECTION("a navigation name carries the technology that wrote it")
  {
    CHECK(technology_name(form::technology::root_ttree) == "root_ttree");
    CHECK(technology_name(form::technology::root_rntuple) == "root_rntuple");
    CHECK(technology_name(form::technology::id{}) == "generic");

    CHECK(navigation_table_name("event", form::technology::root_ttree) ==
          "nav_root_ttree_cells_event");
    CHECK(navigation_table_name("event_segment", form::technology::root_rntuple) ==
          "nav_root_rntuple_cells_event_segment");
    CHECK(navigation_dictionary_name(form::technology::root_ttree) == "nav_root_ttree_products");

    // Navigation name use the reserved prefix.
    CHECK(
      navigation_table_name("event", form::technology::root_ttree).starts_with(navigation_prefix));
    CHECK(navigation_dictionary_name(form::technology::id{}).starts_with(navigation_prefix));
  }
}

namespace {
  // Write one record through persistence.
  void write_record(form::detail::experimental::persistence_writer& writer,
                    std::string const& creator,
                    std::vector<std::string> const& labels,
                    cell_index const& cell,
                    form::technology::id tech = form::technology::id{})
  {
    int payload = 0;
    std::vector<std::pair<placement, std::type_info const*>> containers;
    containers.reserve(labels.size());
    for (auto const& label : labels) {
      containers.emplace_back(product_place(creator, label, tech), &typeid(int));
    }
    writer.create_containers(containers);

    for (auto const& label : labels) {
      writer.register_write(product_place(creator, label, tech), &payload, typeid(int));
    }
    writer.commit_place(product_place(creator, labels.front(), tech), cell);
  }
}

TEST_CASE("navigation: one container per hierarchy, with that hierarchy's layer columns", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  // Two hierarchies in one file.
  write_record(writer, "tracker", {"hits", "tracks"}, event_segment_cell(1, 0));
  write_record(writer, "tracker", {"hits", "tracks"}, event_segment_cell(1, 1));
  write_record(writer, "event_maker", {"summary"}, event_cell(1));
  writer.finalize();

  REQUIRE(store->tables.contains("nav_generic_cells_event_segment"));
  REQUIRE(store->tables.contains("nav_generic_cells_event"));

  // Each hierarchy carries its own layer columns, named as the data cell named them.
  auto const& segment_table = store->tables.at("nav_generic_cells_event_segment");
  CHECK(segment_table.columns == std::vector<std::string>{"event", "segment", "tracker_row"});
  CHECK(segment_table.rows.size() == 2);

  auto const& event_table = store->tables.at("nav_generic_cells_event");
  CHECK(event_table.columns == std::vector<std::string>{"event", "event_maker_row"});
  CHECK(event_table.rows.size() == 1);
}

TEST_CASE("navigation: two technologies in one file each get their own containers", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  write_record(writer, "tracker", {"hits"}, event_cell(1), form::technology::root_ttree);
  write_record(writer, "tracker", {"hits"}, event_cell(1), form::technology::root_rntuple);
  writer.finalize();

  CHECK(store->tables.contains("nav_root_ttree_cells_event"));
  CHECK(store->tables.contains("nav_root_rntuple_cells_event"));
  CHECK(store->tables.contains("nav_root_ttree_products"));
  CHECK(store->tables.contains("nav_root_rntuple_products"));

  CHECK(store->tables.at("nav_root_ttree_cells_event").rows.size() == 1);
  CHECK(store->tables.at("nav_root_rntuple_cells_event").rows.size() == 1);
}

TEST_CASE("navigation: a data cell appears once, with one row per creator", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  // Two creators over the same hierarchy: the wide table gains a column each, not a row each.
  write_record(writer, "tracker", {"hits"}, event_cell(1));
  write_record(writer, "shower", {"showers"}, event_cell(1));
  write_record(writer, "tracker", {"hits"}, event_cell(2));
  writer.finalize();

  auto const& table = store->tables.at("nav_generic_cells_event");
  CHECK(table.columns == std::vector<std::string>{"event", "shower_row", "tracker_row"});
  REQUIRE(table.rows.size() == 2);

  // event 1: both creators wrote, each at its own container's row 0.
  CHECK(table.rows[0] == std::vector<std::string>{num(1), num(0), num(0)});
  // event 2: only the tracker wrote, so the shower column records absence.
  CHECK(table.rows[1] == std::vector<std::string>{num(2), num(invalid_row_id), num(1)});
}

TEST_CASE("navigation: the product dictionary resolves a product to its creator's column", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  write_record(writer, "tracker", {"hits", "tracks"}, event_cell(1));
  write_record(writer, "tracker", {"hits", "tracks"}, event_cell(2));
  writer.finalize();

  auto const& dictionary = store->tables.at("nav_generic_products");
  // Technology is part of the dictionary identity, not a column.
  CHECK(dictionary.columns == std::vector<std::string>{"product_name",
                                                       "creator",
                                                       "container_name",
                                                       "hierarchy_key",
                                                       "navigation_container",
                                                       "navigation_column"});
  // One row per product, not per record.
  REQUIRE(dictionary.rows.size() == 2);

  auto const& hits = dictionary.rows[0];
  CHECK(hits[0] == "hits");
  CHECK(hits[1] == "tracker");
  CHECK(hits[2] == "tracker/hits");
  CHECK(hits[3] == "event");
  // The navigation container names the object that actually exists on disk.
  CHECK(hits[4] == "nav_generic_cells_event");
  CHECK(hits[5] == "tracker_row");
}

TEST_CASE("navigation: a creator whose products disagree on their row is rejected", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  int payload = 0;
  std::vector<std::pair<placement, std::type_info const*>> containers{
    {product_place("tracker", "hits"), &typeid(int)},
    {product_place("tracker", "tracks"), &typeid(int)}};
  writer.create_containers(containers);

  // Force the two product containers to have different rows.
  store->set_next_row("tracker/tracks", 7);
  writer.register_write(product_place("tracker", "hits"), &payload, typeid(int));
  writer.register_write(product_place("tracker", "tracks"), &payload, typeid(int));

  CHECK_THROWS_AS(writer.commit_place(product_place("tracker", "hits"), event_cell(1)),
                  std::runtime_error);
}

TEST_CASE("navigation: a failed product write abandons the whole record", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  int payload = 0;
  std::vector<std::pair<placement, std::type_info const*>> containers{
    {product_place("tracker", "hits"), &typeid(int)},
    {product_place("tracker", "tracks"), &typeid(int)}};
  writer.create_containers(containers);

  writer.register_write(product_place("tracker", "hits"), &payload, typeid(int));

  // The backend fails on the record's second product.
  store->throw_on_fill = "tracker/tracks";
  CHECK_THROWS_AS(writer.register_write(product_place("tracker", "tracks"), &payload, typeid(int)),
                  std::runtime_error);
  store->throw_on_fill.clear();

  // The first product's pending write was discarded because a partial record must not be navigable
  writer.commit_place(product_place("tracker", "hits"), event_cell(1));
  writer.finalize();
  CHECK_FALSE(store->tables.contains("nav_generic_cells_event"));
}

TEST_CASE("navigation: a creator writing one data cell twice is rejected", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  // The wide table holds one row per creator per data cell, so the second write has nowhere to go.
  write_record(writer, "tracker", {"hits"}, event_cell(1));
  CHECK_THROWS_AS(write_record(writer, "tracker", {"hits"}, event_cell(1)), std::runtime_error);
}

TEST_CASE("navigation: a layer column colliding with a creator column is rejected", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  // A layer named "tracker_row" yields the same column name as creator "tracker" does.
  cell_index const cell{
    .id = "[tracker_row:1]", .layer_names = {"tracker_row"}, .layer_values = {1}};
  write_record(writer, "tracker", {"hits"}, cell);

  // The clash is only visible once every creator is known, so finalize is where it throws.
  CHECK_THROWS_AS(writer.finalize(), std::runtime_error);
}

TEST_CASE("navigation: the job cell is a hierarchy with no layer columns", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  // The job cell has no layer columns.
  write_record(writer, "tracker", {"summary"}, cell_index{.id = "[]"});
  writer.finalize();

  auto const& table = store->tables.at("nav_generic_cells_job");
  CHECK(table.columns == std::vector<std::string>{"tracker_row"});
  REQUIRE(table.rows.size() == 1);
  CHECK(table.rows[0] == std::vector<std::string>{num(0)});
}

TEST_CASE("navigation: one hierarchy key from different layers is rejected", "[form]")
{
  // Distinct hierarchies must not share the same navigation table.
  cell_index const job_cell{.id = "[]"};
  cell_index const named_job{.id = "[job:1]", .layer_names = {"job"}, .layer_values = {1}};

  SECTION("job cell first")
  {
    auto spy = std::make_unique<spy_storage_writer>();
    form::detail::experimental::persistence_writer writer{std::move(spy)};

    write_record(writer, "tracker", {"hits"}, job_cell);
    CHECK_THROWS_AS(write_record(writer, "tracker", {"hits"}, named_job), std::runtime_error);
  }

  SECTION("named layer first")
  {
    auto spy = std::make_unique<spy_storage_writer>();
    form::detail::experimental::persistence_writer writer{std::move(spy)};

    write_record(writer, "tracker", {"hits"}, named_job);
    CHECK_THROWS_AS(write_record(writer, "tracker", {"hits"}, job_cell), std::runtime_error);
  }

  SECTION("one layer name collides with two joined ones")
  {
    auto spy = std::make_unique<spy_storage_writer>();
    form::detail::experimental::persistence_writer writer{std::move(spy)};

    cell_index const joined{
      .id = "[event_segment:1]", .layer_names = {"event_segment"}, .layer_values = {1}};
    write_record(writer, "tracker", {"hits"}, joined);
    CHECK_THROWS_AS(write_record(writer, "tracker", {"hits"}, event_segment_cell(1, 0)),
                    std::runtime_error);
  }

  SECTION("two joined layer names collide with one")
  {
    auto spy = std::make_unique<spy_storage_writer>();
    form::detail::experimental::persistence_writer writer{std::move(spy)};

    cell_index const joined{
      .id = "[event_segment:1]", .layer_names = {"event_segment"}, .layer_values = {1}};
    write_record(writer, "tracker", {"hits"}, event_segment_cell(1, 0));
    CHECK_THROWS_AS(write_record(writer, "tracker", {"hits"}, joined), std::runtime_error);
  }
}

TEST_CASE("navigation: a cell whose layer vectors disagree is rejected", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  // Layer names and values must line up; otherwise the row cannot be built.
  cell_index const broken{
    .id = "[event:1]", .layer_names = {"event", "segment"}, .layer_values = {1}};
  CHECK_THROWS_AS(write_record(writer, "tracker", {"hits"}, broken), std::runtime_error);
}

TEST_CASE("navigation: the reserved container prefix is rejected", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  // Reserve the "nav_" prefix for navigation containers.
  std::vector<std::pair<placement, std::type_info const*>> containers{
    {product_place("nav_generic_cells_event", "hits"), &typeid(int)}};
  CHECK_THROWS_AS(writer.create_containers(containers), std::runtime_error);
}

TEST_CASE("navigation: finalize is idempotent", "[form]")
{
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  write_record(writer, "tracker", {"hits"}, event_cell(1));
  writer.finalize();
  writer.finalize();

  // Written once, not twice.
  CHECK(store->tables.at("nav_generic_cells_event").rows.size() == 1);
  CHECK(store->tables.at("nav_generic_cells_event").columns ==
        std::vector<std::string>{"event", "tracker_row"});
}

TEST_CASE("navigation: the per-creator index is still written", "[form]")
{
  // Keep the existing index for the current read path.
  auto spy = std::make_unique<spy_storage_writer>();
  auto* store = spy.get();
  form::detail::experimental::persistence_writer writer{std::move(spy)};

  write_record(writer, "tracker", {"hits"}, event_cell(1));
  writer.finalize();

  CHECK(std::find(store->filled_containers.begin(),
                  store->filled_containers.end(),
                  "tracker/index") != store->filled_containers.end());
}
