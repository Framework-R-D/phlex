#include <catch2/catch_test_macros.hpp>
#include "core/token.hpp"
#include "storage/storage_association.hpp"
#include "storage/storage_associative_container.hpp"
#include "storage/storage_container.hpp"
#include "storage/storage_file.hpp"
#include "storage/istorage.hpp"
#include "persistence/persistence.hpp"
#include "form/config.hpp"
#include "util/factories.hpp"

#include <memory>
#include <stdexcept>

using namespace form::detail::experimental;

TEST_CASE("Token basics", "[form]") {
    Token t("file.root", "container", 1, 42);
    CHECK(t.fileName() == "file.root");
    CHECK(t.containerName() == "container");
    CHECK(t.technology() == 1);
    CHECK(t.id() == 42);
}

TEST_CASE("Storage_File basics", "[form]") {
    Storage_File f("test.root", 'o');
    CHECK(f.name() == "test.root");
    CHECK(f.mode() == 'o');
    CHECK_THROWS_AS(f.setAttribute("key", "value"), std::runtime_error);
}

TEST_CASE("Storage_Container basics", "[form]") {
    Storage_Container c("my_container");
    CHECK(c.name() == "my_container");

    auto f = std::make_shared<Storage_File>("test.root", 'o');
    c.setFile(f);

    c.setupWrite(typeid(int));
    c.fill(nullptr);
    c.commit();

    void const* data = nullptr;
    CHECK_FALSE(c.read(1, &data, typeid(int)));

    CHECK_THROWS_AS(c.setAttribute("key", "value"), std::runtime_error);
}

TEST_CASE("Storage_Association basics", "[form]") {
    Storage_Association a("my_assoc/extra");
    CHECK(a.name() == "my_assoc"); // maybe_remove_suffix should remove /extra

    a.setAttribute("key", "value"); // Storage_Association overrides setAttribute to do nothing
}

TEST_CASE("Storage_Associative_Container basics", "[form]") {
    SECTION("With slash") {
        Storage_Associative_Container c("parent/child");
        CHECK(c.top_name() == "parent");
        CHECK(c.col_name() == "child");
    }
    SECTION("Without slash") {
        Storage_Associative_Container c("no_slash");
        CHECK(c.top_name() == "no_slash");
        CHECK(c.col_name() == "Main");
    }

    Storage_Associative_Container c("p/c");
    auto parent = std::make_shared<Storage_Container>("p");
    c.setParent(parent);
}

TEST_CASE("Factories fallback", "[form]") {
    auto f = createFile(0, "test.root", 'o');
    CHECK(dynamic_cast<Storage_File*>(f.get()) != nullptr);

    auto a = createAssociation(0, "assoc");
    CHECK(dynamic_cast<Storage_Association*>(a.get()) != nullptr);

    auto c = createContainer(0, "cont");
    CHECK(dynamic_cast<Storage_Container*>(c.get()) != nullptr);
}

TEST_CASE("Storage basic operations", "[form]") {
    auto storage = createStorage();
    REQUIRE(storage != nullptr);

    form::experimental::config::tech_setting_config settings;

    std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
    auto p = std::make_unique<Placement>("file.root", "cont", 0);
    containers.emplace(std::move(p), &typeid(int));

    storage->createContainers(containers, settings);

    Placement p2("file.root", "cont", 0);
    int data = 42;
    storage->fillContainer(p2, &data, typeid(int));
    storage->commitContainers(p2);

    Token token("file.root", "cont", 0, 1);
    void const* read_data = nullptr;
    storage->readContainer(token, &read_data, typeid(int), settings);

    int index = storage->getIndex(token, "some_id", settings);
    CHECK(index == 0);
}

TEST_CASE("Persistence basic operations", "[form]") {
    auto p = createPersistence();
    REQUIRE(p != nullptr);

    using namespace form::experimental::config;
    output_item_config out_cfg;
    out_cfg.addItem("prod", "file.root", 0);
    out_cfg.addItem("parent/child", "file.root", 0);
    p->configureOutputItems(out_cfg);

    tech_setting_config tech_cfg;
    p->configureTechSettings(tech_cfg);

    SECTION("Full Lifecycle") {
        std::map<std::string, std::type_info const*> products;
        products["prod"] = &typeid(int);
        products["parent/child"] = &typeid(double);
        CHECK_NOTHROW(p->createContainers("my_creator", products));

        int val = 42;
        CHECK_NOTHROW(p->registerWrite("my_creator", "prod", &val, typeid(int)));
        CHECK_NOTHROW(p->commitOutput("my_creator", "event_1"));

        void const* data = nullptr;
        // This will call getToken -> getIndex (returns 0 for Storage_Container) -> readContainer
        CHECK_NOTHROW(p->read("my_creator", "prod", "event_1", &data, typeid(int)));
    }

    SECTION("Errors") {
        int val = 42;
        CHECK_THROWS_AS(p->registerWrite("my_creator", "unknown", &val, typeid(int)), std::runtime_error);
    }
}

TEST_CASE("form::experimental::config tests", "[form]") {
    using namespace form::experimental::config;

    SECTION("output_item_config") {
        output_item_config cfg;
        cfg.addItem("prod1", "file1.root", 1);

        auto item = cfg.findItem("prod1");
        REQUIRE(item.has_value());
        CHECK(item->product_name == "prod1");

        CHECK_FALSE(cfg.findItem("nonexistent").has_value());
    }

    SECTION("tech_setting_config") {
        tech_setting_config cfg;
        cfg.file_settings[1]["file1.root"] = {{"attr", "val"}};
        cfg.container_settings[1]["cont1"] = {{"cattr", "cval"}};

        auto ftable = cfg.getFileTable(1, "file1.root");
        REQUIRE(ftable.size() == 1);
        CHECK(ftable[0].first == "attr");
        CHECK(ftable[0].second == "val");

        auto ctable = cfg.getContainerTable(1, "cont1");
        REQUIRE(ctable.size() == 1);
        CHECK(ctable[0].first == "cattr");
        CHECK(ctable[0].second == "cval");
    }
}
