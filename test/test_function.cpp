#include "gtest/gtest.h"

#include "ju/sqlite/function.hpp"

int func1(int a) { return a + 1; }

struct func2 {
  int operator()(int a) { return a + 2; }
};

static bool func3_dtor_called = false;
struct func3 {
  int call_count;

  explicit func3(int initial_count) : call_count(initial_count) {
    func3_dtor_called = false;
  }

  int operator()(int a) {
    ++call_count;
    return a + call_count;
  }

  ~func3() { func3_dtor_called = true; }
};

TEST(JU_SQLITE, function_pointer) {
  using namespace ju::sqlite;

  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  register_function(db, "func1", SQLITE_DETERMINISTIC, &func1);

  // Prepare a statement to call the function
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT func1(42);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 43);

  // Clean up
  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, function_object_stateless) {
  using namespace ju::sqlite;

  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  create_function<func2>(db, "func2", SQLITE_DETERMINISTIC);

  // Prepare a statement to call the function
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT func2(42);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 44);

  // Clean up
  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, function_object_stateful) {
  using namespace ju::sqlite;

  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  create_function<func3>(db, "func3", 0, 0);

  // Prepare a statement to call the function
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT func3(42);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 43);
  ASSERT_FALSE(func3_dtor_called); // Destructor should not be called yet

  // Call the function again to check statefulness
  sqlite3_reset(st);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 44);
  ASSERT_FALSE(func3_dtor_called); // Destructor should still not be called

  // Clean up
  sqlite3_finalize(st);
  ASSERT_FALSE(func3_dtor_called); // Destructor should still not be called
  sqlite3_close(db);
  ASSERT_TRUE(func3_dtor_called);
}

TEST(JU_SQLITE, function_lambda) {
  using namespace ju::sqlite;

  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  register_function(db, "func_lambda", SQLITE_DETERMINISTIC, [](int a) { return a + 3; });

  // Prepare a statement to call the function
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT func_lambda(42);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 45);

  // Clean up
  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, function_can_access_context) {
  using namespace ju::sqlite;

  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  register_function(
      db, "func_context", SQLITE_DETERMINISTIC, [](context_raw *ctx, int a) {
        // Access the context_raw and return a value_raw based on it
        sqlite3_result_int(ctx, a + 5);
      });

  // Prepare a statement to call the function
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT func_context(42);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 47);

  // Clean up
  sqlite3_finalize(st);
  sqlite3_close(db);
}
