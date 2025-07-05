#include "gtest/gtest.h"

#include "ju/sqlite/stmt.hpp"

namespace {

// Helper function to create an in-memory database with test data
sqlite3 *create_test_db() {
  sqlite3 *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Create test table
  const char *create_sql = R"(
    CREATE TABLE test_data (
      id INTEGER PRIMARY KEY,
      name TEXT,
      value REAL,
      data BLOB,
      nullable_field INTEGER
    );
  )";

  sqlite3_stmt *stmt = nullptr;
  sqlite3_prepare_v2(db, create_sql, -1, &stmt, nullptr);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Insert test data
  const char *insert_sql = "INSERT INTO test_data (id, name, value, data, "
                           "nullable_field) VALUES (?, ?, ?, ?, ?)";
  sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);

  // Insert some test rows
  for (int i = 1; i <= 5; ++i) {
    sqlite3_bind_int(stmt, 1, i);
    sqlite3_bind_text(
        stmt, 2, ("name" + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, i * 1.5);

    // Simple blob data
    std::string blob_data = "blob" + std::to_string(i);
    sqlite3_bind_blob(
        stmt, 4, blob_data.c_str(), static_cast<int>(blob_data.size()), SQLITE_TRANSIENT);

    // Make some nullable fields null
    if (i % 2 == 0) {
      sqlite3_bind_null(stmt, 5);
    } else {
      sqlite3_bind_int(stmt, 5, i * 10);
    }

    sqlite3_step(stmt);
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
  return db;
}

} // anonymous namespace

TEST(StmtTest, PrepareStmt) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();

  // Test successful preparation
  auto stmt = prepare_stmt(db, "SELECT * FROM test_data");
  EXPECT_TRUE(stmt);
  EXPECT_NE(stmt.handle(), nullptr);

  // Test failed preparation
  auto invalid_stmt = prepare_stmt(db, "SELECT * FROM nonexistent_table");
  EXPECT_FALSE(invalid_stmt);
  EXPECT_EQ(invalid_stmt.handle(), nullptr);

  sqlite3_close(db);
}

TEST(StmtTest, StmtConstruction) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();

  // Test default construction
  stmt default_stmt;
  EXPECT_FALSE(default_stmt);
  EXPECT_EQ(default_stmt.handle(), nullptr);

  // Test construction with raw pointer
  sqlite3_stmt *raw_stmt = nullptr;
  sqlite3_prepare_v2(db, "SELECT * FROM test_data", -1, &raw_stmt, nullptr);
  stmt stmt_from_raw(raw_stmt);
  EXPECT_TRUE(stmt_from_raw);
  EXPECT_EQ(stmt_from_raw.handle(), raw_stmt);

  // Test move construction
  stmt moved_stmt = std::move(stmt_from_raw);
  EXPECT_TRUE(moved_stmt);
  EXPECT_FALSE(stmt_from_raw); // Should be null after move

  sqlite3_close(db);
}

TEST(StmtTest, BindParameters) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt =
      prepare_stmt(db, "INSERT INTO test_data (id, name, value) VALUES (?, ?, ?)");
  ASSERT_TRUE(stmt);

  // Test binding multiple parameters at once
  auto err = stmt.bind(100, std::string("test_name"), 3.14);
  EXPECT_EQ(err, error::ok);

  // Test parameter count
  EXPECT_EQ(stmt.param_count(), 3);

  // Test binding individual parameters by position
  err = stmt.bind_at(1, 101);
  EXPECT_EQ(err, error::ok);

  err = stmt.bind_at(2, std::string("another_name"));
  EXPECT_EQ(err, error::ok);

  err = stmt.bind_at(3, 2.71);
  EXPECT_EQ(err, error::ok);

  // Test clearing bindings
  err = stmt.clear_bindings();
  EXPECT_EQ(err, error::ok);

  sqlite3_close(db);
}

TEST(StmtTest, BindParametersByName) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(
      db, "INSERT INTO test_data (id, name, value) VALUES (:id, :name, :value)");
  ASSERT_TRUE(stmt);

  // Test binding by name
  auto err = stmt.bind_name(":id", 200);
  EXPECT_EQ(err, error::ok);

  err = stmt.bind_name(":name", std::string("named_param"));
  EXPECT_EQ(err, error::ok);

  err = stmt.bind_name(":value", 1.41);
  EXPECT_EQ(err, error::ok);

  // Test binding non-existent parameter
  err = stmt.bind_name(":nonexistent", 42);
  EXPECT_EQ(err, error::range);

  sqlite3_close(db);
}

TEST(StmtTest, ParameterNames) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT * FROM test_data WHERE id = :id AND name = :name");
  ASSERT_TRUE(stmt);

  EXPECT_EQ(stmt.param_count(), 2);

  auto param_names = stmt.param_names();
  std::vector<std::string> names(param_names.begin(), param_names.end());

  EXPECT_EQ(names.size(), 2);
  EXPECT_EQ(names[0], ":id");
  EXPECT_EQ(names[1], ":name");

  sqlite3_close(db);
}

TEST(StmtTest, ColumnInfo) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT id, name, value FROM test_data LIMIT 1");
  ASSERT_TRUE(stmt);

  // Need to step once to get column info
  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());

  // Test column count
  EXPECT_EQ(stmt.column_count(), 3);

  // Test column names
  auto column_names = stmt.column_names();
  std::vector<std::string> names(column_names.begin(), column_names.end());

  EXPECT_EQ(names.size(), 3);
  EXPECT_EQ(names[0], "id");
  EXPECT_EQ(names[1], "name");
  EXPECT_EQ(names[2], "value");

  // Test individual column name
  EXPECT_EQ(stmt.column_name(0), "id");
  EXPECT_EQ(stmt.column_name(1), "name");
  EXPECT_EQ(stmt.column_name(2), "value");

  // Test column types
  EXPECT_EQ(stmt.column_type(0), value_type::int_);
  EXPECT_EQ(stmt.column_type(1), value_type::text);
  EXPECT_EQ(stmt.column_type(2), value_type::real);

  auto column_types = stmt.column_types();
  std::vector<value_type> types(column_types.begin(), column_types.end());

  EXPECT_EQ(types.size(), 3);
  EXPECT_EQ(types[0], value_type::int_);
  EXPECT_EQ(types[1], value_type::text);
  EXPECT_EQ(types[2], value_type::real);

  sqlite3_close(db);
}

TEST(StmtTest, StmtRowAccess) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT id, name, value FROM test_data WHERE id = 1");
  ASSERT_TRUE(stmt);

  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());

  const auto &row = *it;

  // Test individual column access
  EXPECT_EQ(row.get<int>(0), 1);
  EXPECT_EQ(row.get<std::string>(1), "name1");
  EXPECT_DOUBLE_EQ(row.get<double>(2), 1.5);

  // Test tuple access
  auto [id, name, value] = row.get<int, std::string, double>();
  EXPECT_EQ(id, 1);
  EXPECT_EQ(name, "name1");
  EXPECT_DOUBLE_EQ(value, 1.5);

  // Test implicit conversion (first column)
  int first_col = row;
  EXPECT_EQ(first_col, 1);

  sqlite3_close(db);
}

TEST(StmtTest, IteratorInterface) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT id FROM test_data ORDER BY id");
  ASSERT_TRUE(stmt);

  // Test range-based for loop
  std::vector<int> ids;
  for (const auto &row : stmt) {
    ids.push_back(row.get<int>(0));
  }

  EXPECT_EQ(ids.size(), 5);
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(ids[i], static_cast<int>(i + 1));
  }

  // Test manual iteration
  stmt.reset();
  auto it = stmt.begin();
  int count = 0;
  while (it != stmt.end()) {
    ++count;
    ++it;
  }
  EXPECT_EQ(count, 5);

  sqlite3_close(db);
}

TEST(StmtTest, Reset) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT COUNT(*) FROM test_data");
  ASSERT_TRUE(stmt);

  // Execute once
  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count1 = (*it).get<int>(0);
  EXPECT_EQ(count1, 5);

  // Reset and execute again
  auto err = stmt.reset();
  EXPECT_EQ(err, error::ok);

  it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count2 = (*it).get<int>(0);
  EXPECT_EQ(count2, 5);

  sqlite3_close(db);
}

TEST(StmtTest, ResetWithClearBindings) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT COUNT(*) FROM test_data WHERE id = ?");
  ASSERT_TRUE(stmt);

  // Bind parameter and execute
  stmt.bind(1);
  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count1 = (*it).get<int>(0);
  EXPECT_EQ(count1, 1);

  // Reset with clear bindings
  auto err = stmt.reset(true);
  EXPECT_EQ(err, error::ok);

  // Execute again - should return 0 since parameter is now unbound (NULL)
  it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count2 = (*it).get<int>(0);
  EXPECT_EQ(count2, 0); // NULL parameter means no matches

  sqlite3_close(db);
}

TEST(StmtTest, ClearBindingsEffect) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT COUNT(*) FROM test_data WHERE id = ?");
  ASSERT_TRUE(stmt);

  // Bind parameter and execute
  stmt.bind(1);
  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count1 = (*it).get<int>(0);
  EXPECT_EQ(count1, 1);

  // Clear bindings explicitly
  auto err = stmt.clear_bindings();
  EXPECT_EQ(err, error::ok);

  // Reset and execute again
  stmt.reset();
  it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count2 = (*it).get<int>(0);
  EXPECT_EQ(count2, 0); // Parameter is NULL, so no matches

  sqlite3_close(db);
}

TEST(StmtTest, Exec) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt =
      prepare_stmt(db, "INSERT INTO test_data (id, name, value) VALUES (?, ?, ?)");
  ASSERT_TRUE(stmt);

  // Bind parameters
  stmt.bind(100, std::string("exec_test"), 99.9);

  // Execute
  auto err = stmt.exec();
  EXPECT_EQ(err, error::done);

  // Verify the insertion
  auto select_stmt = prepare_stmt(db, "SELECT COUNT(*) FROM test_data WHERE id = 100");
  auto it = select_stmt.begin();
  ASSERT_NE(it, select_stmt.end());
  int count = (*it).get<int>(0);
  EXPECT_EQ(count, 1);

  sqlite3_close(db);
}

TEST(StmtTest, ExecWithReset) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt =
      prepare_stmt(db, "INSERT INTO test_data (id, name, value) VALUES (?, ?, ?)");
  ASSERT_TRUE(stmt);

  // Execute with reset
  stmt.bind(101, std::string("reset_test"), 88.8);
  auto err = stmt.exec(true);
  EXPECT_EQ(err, error::ok);

  // Should be able to execute again after reset
  stmt.bind(102, std::string("reset_test2"), 77.7);
  err = stmt.exec(true, true); // reset with clear bindings
  EXPECT_EQ(err, error::ok);

  sqlite3_close(db);
}

TEST(StmtTest, HandleNullValues) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT id, nullable_field FROM test_data WHERE id = 2");
  ASSERT_TRUE(stmt);

  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());

  // Test column type for null
  EXPECT_EQ(stmt.column_type(1), value_type::null);

  sqlite3_close(db);
}

TEST(StmtTest, BindNullValues) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(
      db, "INSERT INTO test_data (id, name, nullable_field) VALUES (?, ?, ?)");
  ASSERT_TRUE(stmt);

  // Bind with null
  auto err = stmt.bind(200, std::string("null_test"), nullptr);
  EXPECT_EQ(err, error::ok);

  err = stmt.exec();
  EXPECT_EQ(err, error::done);

  // Verify null was inserted
  auto select_stmt =
      prepare_stmt(db, "SELECT nullable_field FROM test_data WHERE id = 200");
  auto it = select_stmt.begin();
  ASSERT_NE(it, select_stmt.end());

  EXPECT_EQ(select_stmt.column_type(0), value_type::null);

  sqlite3_close(db);
}

TEST(StmtTest, BlobHandling) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT data FROM test_data WHERE id = 1");
  ASSERT_TRUE(stmt);

  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());

  // Test column type for blob
  EXPECT_EQ(stmt.column_type(0), value_type::blob);

  sqlite3_close(db);
}

TEST(StmtTest, EmptyResultSet) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT * FROM test_data WHERE id = 999");
  ASSERT_TRUE(stmt);

  // Test that iterator immediately equals end for empty result
  auto it = stmt.begin();
  EXPECT_EQ(it, stmt.end());

  // Test range-based for loop with empty result
  int count = 0;
  for ([[maybe_unused]] const auto &row : stmt) {
    ++count;
  }
  EXPECT_EQ(count, 0);

  sqlite3_close(db);
}

TEST(StmtTest, MultipleExecutions) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT COUNT(*) FROM test_data WHERE id <= ?");
  ASSERT_TRUE(stmt);

  // First execution
  stmt.bind(3);
  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count1 = (*it).get<int>(0);
  EXPECT_EQ(count1, 3);

  // Reset and execute again with different parameter
  stmt.reset();
  stmt.bind(2);
  it = stmt.begin();
  ASSERT_NE(it, stmt.end());
  int count2 = (*it).get<int>(0);
  EXPECT_EQ(count2, 2);

  sqlite3_close(db);
}

TEST(StmtTest, TemplateBindPosition) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt =
      prepare_stmt(db, "INSERT INTO test_data (id, name, value) VALUES (?, ?, ?)");
  ASSERT_TRUE(stmt);

  // Test template-based position binding
  auto err = stmt.bind<1>(300);
  EXPECT_EQ(err, error::ok);

  err = stmt.bind<2>(std::string("template_test"));
  EXPECT_EQ(err, error::ok);

  err = stmt.bind<3>(42.0);
  EXPECT_EQ(err, error::ok);

  err = stmt.exec();
  EXPECT_EQ(err, error::done);

  sqlite3_close(db);
}

TEST(StmtTest, ErrorHandling) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT * FROM test_data WHERE id = ?");
  ASSERT_TRUE(stmt);

  // Test binding with invalid parameter position
  auto err = stmt.bind_at(99, 42); // Position out of range
  EXPECT_EQ(err, error::range);

  sqlite3_close(db);
}

TEST(StmtTest, IteratorStateChecking) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT id FROM test_data WHERE id = 1");
  ASSERT_TRUE(stmt);

  auto it = stmt.begin();
  ASSERT_NE(it, stmt.end());

  // Check iterator state - when there's a row, state should be error::row
  EXPECT_EQ(it.state(), error::row);

  // Move to end
  ++it;
  EXPECT_EQ(it, stmt.end());
  EXPECT_EQ(it.state(), error::done);

  sqlite3_close(db);
}

TEST(StmtTest, LargeDataHandling) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "INSERT INTO test_data (id, name) VALUES (?, ?)");
  ASSERT_TRUE(stmt);

  // Test with large string
  std::string large_text(10000, 'A');
  auto err = stmt.bind(999, large_text);
  EXPECT_EQ(err, error::ok);

  err = stmt.exec();
  EXPECT_EQ(err, error::done);

  // Verify the large text was stored correctly
  auto select_stmt = prepare_stmt(db, "SELECT name FROM test_data WHERE id = 999");
  auto it = select_stmt.begin();
  ASSERT_NE(it, select_stmt.end());

  std::string retrieved = (*it).get<std::string>(0);
  EXPECT_EQ(retrieved.size(), 10000);
  EXPECT_EQ(retrieved[0], 'A');
  EXPECT_EQ(retrieved[9999], 'A');

  sqlite3_close(db);
}

TEST(StmtTest, Int64Handling) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt =
      prepare_stmt(db, "INSERT INTO test_data (id, nullable_field) VALUES (?, ?)");
  ASSERT_TRUE(stmt);

  // Test with large int64 values
  int64_t large_int = 9223372036854775807LL; // max int64
  auto err = stmt.bind(1000, large_int);
  EXPECT_EQ(err, error::ok);

  err = stmt.exec();
  EXPECT_EQ(err, error::done);

  // Verify the int64 was stored correctly
  auto select_stmt =
      prepare_stmt(db, "SELECT nullable_field FROM test_data WHERE id = 1000");
  auto it = select_stmt.begin();
  ASSERT_NE(it, select_stmt.end());

  int64_t retrieved = (*it).get<int64_t>(0);
  EXPECT_EQ(retrieved, large_int);

  sqlite3_close(db);
}

TEST(StmtTest, DoubleHandling) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "INSERT INTO test_data (id, value) VALUES (?, ?)");
  ASSERT_TRUE(stmt);

  // Test with precise double values
  double precise_value = 3.141592653589793;
  auto err = stmt.bind(1001, precise_value);
  EXPECT_EQ(err, error::ok);

  err = stmt.exec();
  EXPECT_EQ(err, error::done);

  // Verify the double was stored correctly
  auto select_stmt = prepare_stmt(db, "SELECT value FROM test_data WHERE id = 1001");
  auto it = select_stmt.begin();
  ASSERT_NE(it, select_stmt.end());

  double retrieved = (*it).get<double>(0);
  EXPECT_DOUBLE_EQ(retrieved, precise_value);

  sqlite3_close(db);
}

TEST(StmtTest, StmtSentinelComparison) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  auto stmt = prepare_stmt(db, "SELECT id FROM test_data LIMIT 1");
  ASSERT_TRUE(stmt);

  auto it = stmt.begin();
  stmt_sentinel sentinel;

  // Test that non-end iterator is not equal to sentinel
  EXPECT_NE(it, sentinel);
  EXPECT_FALSE(it == sentinel);

  // Move to end and test equality
  ++it;
  EXPECT_EQ(it, sentinel);
  EXPECT_TRUE(it == sentinel);

  sqlite3_close(db);
}
