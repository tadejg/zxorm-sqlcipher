#include <gtest/gtest.h>
#include <filesystem>
#include <regex>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object {
    int _id = 0;
    std::string _name;
    int _someId = 0;
    std::string _someText;
    float _someFloat;
    auto getId() { return _id; }
    void setId(int id) { _id = id; }
    auto getName() { return _name; }
    void setName(std::string name) { _name = name; }
};

struct Object2 {
    int _id = 0;
    std::string _name;
    int _someId = 0;
    std::string _someText;
    float _someFloat;
    auto getId() { return _id; }
    void setId(int id) { _id = id; }
    auto getName() { return _name; }
    void setName(std::string name) { _name = name; }
};

struct Object3 {
    int _id = 0;
    std::string _name;
    int _someId = 0;
    std::string _someText;
    float _someFloat;
    auto getId() { return _id; }
    void setId(int id) { _id = id; }
    auto getName() { return _name; }
    void setName(std::string name) { _name = name; }
};

using table_t = Table<"test", Object,
    Column<"id", &Object::_id>,
    Column<"name", &Object::_name>
        > ;

using tablepriv_t = Table<"test_private", Object2,
    ColumnPrivate<"id",&Object2::getId, &Object2::setId>,
    ColumnPrivate<"name", &Object2::getName, &Object2::setName>
        > ;

using table_with_column_constraints_t = Table<"test_constraints", Object3,
    Column<"id", &Object3::_id, PrimaryKey<conflict_t::abort>>,
    Column<"name", &Object3::_name, NotNull<>, Unique<>>,
    Column<"text", &Object3::_someText, Unique<conflict_t::replace>>,
    Column<"float", &Object3::_someFloat>,
    Column<"someId", &Object3::_someId, ForeignKey<Reference<"test", "id">, action_t::cascade, action_t::restrict>>
        >;

using MyConnection = Connection<table_t, tablepriv_t, table_with_column_constraints_t>;

class TableTest : public ::testing::Test {
    protected:
    void SetUp() override {
        auto createdConn = MyConnection::create("test.db", 0, 0, &logger);
        if (!createdConn) {
            throw "Unable to open connection";
        }

        myConn = std::make_shared<MyConnection>(createdConn.value());
    }

    std::shared_ptr<MyConnection> myConn;

    void TearDown() override {
        myConn = nullptr;
        std::filesystem::remove("test.db");
    }
};


TEST_F(TableTest, Columns) {
    table_t table;

    ASSERT_EQ(table.columnName(0), "id");
    ASSERT_EQ(table.columnName(1), "name");
}

TEST_F(TableTest, ColumnsPrivate) {
    tablepriv_t table;
    ASSERT_EQ(table.columnName(0), "id");
    ASSERT_EQ(table.columnName(1), "name");
}

TEST_F(TableTest, nColumns) {
    ASSERT_EQ(table_t::nColumns, 2);
}

TEST_F(TableTest, CreateTableQuery) {
    std::string query = table_t::createTableQuery(false);
    std::regex reg ("\\s+");
    auto trimmed = std::regex_replace (query, reg, " ");
    ASSERT_EQ(trimmed, "CREATE TABLE test ( `id` INTEGER NOT NULL ON CONFLICT ABORT, `name` TEXT NOT NULL ON CONFLICT ABORT ); ");
    std::string same = tablepriv_t::createTableQuery(false);
    std::regex reg2 ("_private");
    same = std::regex_replace (same, reg2, "");
    ASSERT_EQ(same, query);
}

TEST_F(TableTest, CreateWithConstraintsTableQuery) {
    std::string query = table_with_column_constraints_t::createTableQuery(false);
    std::regex reg ("\\s+");
    auto trimmed = std::regex_replace (query, reg, " ");
    std::string expected = "CREATE TABLE test_constraints ( "
        "`id` INTEGER NOT NULL ON CONFLICT ABORT PRIMARY KEY ON CONFLICT ABORT, "
        "`name` TEXT NOT NULL ON CONFLICT ABORT UNIQUE ON CONFLICT ABORT, "
        "`text` TEXT NOT NULL ON CONFLICT ABORT UNIQUE ON CONFLICT REPLACE, "
        "`float` REAL NOT NULL ON CONFLICT ABORT, "
        "`someId` INTEGER NOT NULL ON CONFLICT ABORT REFERENCES `test` (`id`) ON UPDATE CASCADE ON DELETE RESTRICT "
        "); ";
    ASSERT_EQ(trimmed, expected);
}
