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

TEST(JU_SQLITE, function_void_return_sets_result) {
  using namespace ju::sqlite;
  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Register a function that returns void and sets result via context
  auto void_func = [](context_raw *ctx, int a, int b) { sqlite3_result_int(ctx, a * b); };
  register_function(db, "void_func", SQLITE_DETERMINISTIC, std::move(void_func));

  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT void_func(6, 7);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 42);

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, function_multiple_arguments) {
  using namespace ju::sqlite;
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  auto sum_func = [](int a, int b, int c) { return a + b + c; };
  register_function(db, "sum_func", SQLITE_DETERMINISTIC, std::move(sum_func));

  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT sum_func(10, 20, 12);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 42);

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, function_null_argument) {
  using namespace ju::sqlite;
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Function returns 1 if argument is NULL, else 0
  auto null_check = [](context_raw *ctx, value_raw *v) {
    if (sqlite3_value_type(v) == SQLITE_NULL) {
      sqlite3_result_int(ctx, 1);
    } else {
      sqlite3_result_int(ctx, 0);
    }
  };
  register_function(db, "is_null", SQLITE_DETERMINISTIC, std::move(null_check));

  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT is_null(NULL);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 1);
  sqlite3_finalize(st);

  sqlite3_prepare_v2(db, "SELECT is_null(123);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 0);
  sqlite3_finalize(st);

  sqlite3_close(db);
}

TEST(JU_SQLITE, function_exception_propagation) {
  using namespace ju::sqlite;
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Function throws an exception
  auto throwing_func = [](int) -> int { throw std::runtime_error("fail"); };
  register_function(db, "throwing_func", SQLITE_DETERMINISTIC, std::move(throwing_func));

  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT throwing_func(1);", -1, &st, nullptr);
  ASSERT_NE(st, nullptr);
  int rc = sqlite3_step(st);
  ASSERT_EQ(rc, SQLITE_ERROR); // Should propagate as error
  sqlite3_finalize(st);
  sqlite3_close(db);
}
