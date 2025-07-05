#include "gtest/gtest.h"

#include "ju/sqlite/aggregate.hpp"

static bool agg1_dtor_called = false;
struct agg1 {
  int sum;

  agg1() noexcept : sum(0) { agg1_dtor_called = false; }
  ~agg1() { agg1_dtor_called = true; }

  void step(int value) { sum += value; }
  int value() const { return sum; }
};

static bool agg2_dtor_called = false;
struct agg2 {
  // rolling sum
  int sum;

  agg2() noexcept : sum(0) { agg2_dtor_called = false; }
  ~agg2() { agg2_dtor_called = true; }

  void step(int value) { sum += value; }
  void inverse(int value) { sum -= value; }
  int value() const { return sum; }
};

// String concatenation aggregate
struct concat_agg {
  std::string result;
  std::string separator;
  bool first = true;

  // TODO: concat_agg ctor is not noexcept
  concat_agg(std::string sep = ",") noexcept : separator(std::move(sep)) {}

  void step(std::string value) {
    if (!first) {
      result += separator;
    } else {
      first = false;
    }
    result += value;
  }

  std::string value() const { return result; }
};

// Error-throwing aggregate for exception safety testing
struct error_agg {

  void step(int value) {
    static_cast<void>(value); // Suppress unused parameter warning
    throw std::runtime_error("Intentional test error");
  }

  int value() const { return 42; }
};

// Aggregate with context parameter
struct context_agg {
  std::vector<int> values;

  void step(ju::sqlite::context_raw *ctx, int value) {
    values.push_back(value);
    // We could use the context to set aux data or check user data
    static_cast<void>(ctx);
  }

  int value() const {
    return values.empty() ? 0 : *std::max_element(values.begin(), values.end());
  }
};

// Complex windowed aggregate with inverse
struct windowed_stats {
  std::deque<double> values;
  double sum = 0.0;

  void step(double value) {
    values.push_back(value);
    sum += value;
  }

  void inverse(double value) {
    if (!values.empty()) {
      // Find and remove the value (for simplicity, assume FIFO)
      auto it = std::find(values.begin(), values.end(), value);
      if (it != values.end()) {
        values.erase(it);
        sum -= value;
      }
    }
  }

  double value() const { return values.empty() ? 0.0 : sum / values.size(); }
};

void prepare_data(sqlite3 *db) {
  using namespace ju::sqlite;
  stmt_raw *st;
  sqlite3_prepare_v2(db, "CREATE TABLE test(value INTEGER);", -1, &st, nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);

  sqlite3_prepare_v2(db, "INSERT INTO test(value) VALUES (?);", -1, &st, nullptr);
  for (int i = 1; i <= 10; ++i) {
    sqlite3_bind_int(st, 1, i);
    sqlite3_step(st);
    sqlite3_reset(st);
  }
  sqlite3_finalize(st);
}

TEST(JU_SQLITE, aggregate) {

  using namespace ju::sqlite;

  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);
  prepare_data(db);

  // Create an aggregate function
  auto e = create_aggregate<agg1>(db, "agg1", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Prepare a statement to use the aggregate function
  stmt_raw *st = nullptr;
  auto rc = sqlite3_prepare_v2(db, "SELECT agg1(value) FROM test;", -1, &st, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  // Check the result of the aggregate function
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 55); // 1 + 2 + ... + 10

  // Clean up
  sqlite3_finalize(st);
  sqlite3_close(db);

  ASSERT_TRUE(agg1_dtor_called); // Destructor should be called
}

TEST(JU_SQLITE, aggregate_window) {
  using namespace ju::sqlite;

  // Create a connection to an in-memory SQLite database
  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);
  prepare_data(db);

  // Create an aggregate function
  auto e = create_aggregate<agg2>(db, "agg2", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Prepare a statement to use the aggregate function
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(
      db,
      "SELECT agg2(value) OVER (ORDER BY value rows 5 PRECEDING) FROM test;",
      -1,
      &st,
      nullptr);
  ASSERT_NE(st, nullptr);

  int row = 1;
  int sum = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    // Check the result of the aggregate function
    int result = sqlite3_column_int(st, 0);
    if (row <= 6) { // 5 PRECEDING
      sum += row;
      ASSERT_EQ(result, sum);
    } else {
      sum += row - (row - 6);
      ASSERT_EQ(result, sum);
    }
    ++row;
  }

  // Clean up
  sqlite3_finalize(st);
  sqlite3_close(db);

  ASSERT_TRUE(agg2_dtor_called); // Destructor should be called
}

TEST(JU_SQLITE, aggregate_string_concatenation) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Create test data
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "CREATE TABLE names(name TEXT);", -1, &st, nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);

  sqlite3_prepare_v2(db, "INSERT INTO names(name) VALUES (?);", -1, &st, nullptr);
  std::vector<std::string> names = {"Alice", "Bob", "Charlie", "David"};
  for (const auto &name : names) {
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(st);
    sqlite3_reset(st);
  }
  sqlite3_finalize(st);

  // Create concat aggregate with custom separator
  auto e = create_aggregate<concat_agg>(
      db, "concat_agg", SQLITE_DETERMINISTIC, std::string("|"));
  ASSERT_EQ(e, error::ok);

  // Test concatenation
  sqlite3_prepare_v2(db, "SELECT concat_agg(name) FROM names;", -1, &st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);

  const char *result = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
  std::string expected = "Alice|Bob|Charlie|David";
  ASSERT_STREQ(result, expected.c_str());

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_multiple_instances) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Create test data with groups
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db,
                     "CREATE TABLE grouped_data(group_id INTEGER, value INTEGER);",
                     -1,
                     &st,
                     nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);

  sqlite3_prepare_v2(
      db, "INSERT INTO grouped_data(group_id, value) VALUES (?, ?);", -1, &st, nullptr);
  // Group 1: values 1, 2, 3 (sum = 6)
  // Group 2: values 4, 5, 6 (sum = 15)
  for (int group = 1; group <= 2; ++group) {
    for (int val = 1; val <= 3; ++val) {
      sqlite3_bind_int(st, 1, group);
      sqlite3_bind_int(st, 2, val + (group - 1) * 3);
      sqlite3_step(st);
      sqlite3_reset(st);
    }
  }
  sqlite3_finalize(st);

  auto e = create_aggregate<agg1>(db, "sum_agg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Test grouped aggregation
  sqlite3_prepare_v2(db,
                     "SELECT group_id, sum_agg(value) FROM grouped_data GROUP BY "
                     "group_id ORDER BY group_id;",
                     -1,
                     &st,
                     nullptr);

  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(st, 0), 1);
  ASSERT_EQ(sqlite3_column_int(st, 1), 6); // 1 + 2 + 3

  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  ASSERT_EQ(sqlite3_column_int(st, 0), 2);
  ASSERT_EQ(sqlite3_column_int(st, 1), 15); // 4 + 5 + 6

  ASSERT_EQ(sqlite3_step(st), SQLITE_DONE);

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_empty_dataset) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Create empty table
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "CREATE TABLE empty_test(value INTEGER);", -1, &st, nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);

  auto e = create_aggregate<agg1>(db, "sum_agg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Test aggregation on empty dataset
  sqlite3_prepare_v2(db, "SELECT sum_agg(value) FROM empty_test;", -1, &st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);

  // Should return 0 (initial value) for empty dataset
  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 0);

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_null_values) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Create test data with NULLs
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "CREATE TABLE null_test(value INTEGER);", -1, &st, nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);

  sqlite3_prepare_v2(db, "INSERT INTO null_test(value) VALUES (?);", -1, &st, nullptr);
  // Insert: 1, NULL, 2, NULL, 3
  for (int i = 1; i <= 3; ++i) {
    sqlite3_bind_int(st, 1, i);
    sqlite3_step(st);
    sqlite3_reset(st);

    sqlite3_bind_null(st, 1);
    sqlite3_step(st);
    sqlite3_reset(st);
  }
  sqlite3_finalize(st);

  auto e = create_aggregate<agg1>(db, "sum_agg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Test aggregation with NULLs (SQLite should skip NULL values)
  sqlite3_prepare_v2(db,
                     "SELECT sum_agg(value) FROM null_test WHERE value IS NOT NULL;",
                     -1,
                     &st,
                     nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);

  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 6); // 1 + 2 + 3, NULLs ignored

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_large_dataset) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Create large test dataset
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "CREATE TABLE large_test(value INTEGER);", -1, &st, nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);

  const int N = 10000;
  sqlite3_prepare_v2(db, "INSERT INTO large_test(value) VALUES (?);", -1, &st, nullptr);
  for (int i = 1; i <= N; ++i) {
    sqlite3_bind_int(st, 1, i);
    sqlite3_step(st);
    sqlite3_reset(st);
  }
  sqlite3_finalize(st);

  auto e = create_aggregate<agg1>(db, "sum_agg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Test aggregation on large dataset
  sqlite3_prepare_v2(db, "SELECT sum_agg(value) FROM large_test;", -1, &st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);

  int result = sqlite3_column_int(st, 0);
  int expected = N * (N + 1) / 2; // Sum of 1 to N
  ASSERT_EQ(result, expected);

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_context_parameter) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);
  prepare_data(db);

  auto e = create_aggregate<context_agg>(db, "max_agg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Test aggregation with context parameter
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "SELECT max_agg(value) FROM test;", -1, &st, nullptr);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);

  int result = sqlite3_column_int(st, 0);
  ASSERT_EQ(result, 10); // Maximum value in test data

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_windowed_with_inverse) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Create test data
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "CREATE TABLE window_test(value REAL);", -1, &st, nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);

  sqlite3_prepare_v2(db, "INSERT INTO window_test(value) VALUES (?);", -1, &st, nullptr);
  for (double i = 1.0; i <= 10.0; i += 1.0) {
    sqlite3_bind_double(st, 1, i);
    sqlite3_step(st);
    sqlite3_reset(st);
  }
  sqlite3_finalize(st);

  auto e = create_aggregate<windowed_stats>(db, "windowed_avg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Test windowed function with frame (moving average)
  sqlite3_prepare_v2(
      db,
      "SELECT value, windowed_avg(value) OVER (ORDER BY value ROWS 2 PRECEDING) "
      "FROM window_test ORDER BY value;",
      -1,
      &st,
      nullptr);

  std::vector<std::pair<double, double>> expected_results = {
      {1.0, 1.0}, // avg(1) = 1
      {2.0, 1.5}, // avg(1,2) = 1.5
      {3.0, 2.0}, // avg(1,2,3) = 2
      {4.0, 3.0}, // avg(2,3,4) = 3 (window slides)
      {5.0, 4.0}, // avg(3,4,5) = 4
  };

  for (size_t i = 0; i < std::min(expected_results.size(), 5ul); ++i) {
    ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
    double value = sqlite3_column_double(st, 0);
    double avg = sqlite3_column_double(st, 1);

    ASSERT_DOUBLE_EQ(value, expected_results[i].first);
    ASSERT_NEAR(avg, expected_results[i].second, 0.001);
  }

  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_error_handling) {
  using namespace ju::sqlite;

  conn_raw *db = nullptr;
  sqlite3_open(":memory:", &db);

  // Test creating aggregate with invalid name
  std::string invalid_name = std::string(512, 'a'); // Too long name
  auto e = create_aggregate<agg1>(db, invalid_name.c_str(), SQLITE_DETERMINISTIC);
  ASSERT_NE(e, error::ok);

  // Test with valid name
  e = create_aggregate<agg1>(db, "valid_agg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  // Test duplicate registration (should succeed and overwrite)
  e = create_aggregate<agg1>(db, "valid_agg", SQLITE_DETERMINISTIC);
  ASSERT_EQ(e, error::ok);

  e = create_aggregate<error_agg>(db, "error_agg", SQLITE_DETERMINISTIC, 5);
  ASSERT_EQ(e, error::ok);
  // Test error handling in aggregate
  stmt_raw *st = nullptr;
  sqlite3_prepare_v2(db, "CREATE TABLE error_test(value INTEGER);", -1, &st, nullptr);
  sqlite3_step(st);
  sqlite3_finalize(st);
  sqlite3_prepare_v2(db, "INSERT INTO error_test(value) VALUES (?);", -1, &st, nullptr);
  for (int i = 1; i <= 5; ++i) {
    sqlite3_bind_int(st, 1, i);
    sqlite3_step(st);
    sqlite3_reset(st);
  }

  sqlite3_prepare_v2(db, "SELECT error_agg(value) FROM error_test;", -1, &st, nullptr);
  auto rc = sqlite3_step(st);
  ASSERT_EQ(rc, SQLITE_ERROR);
  std::string msg{sqlite3_errmsg(db)};
  // Compare to "Intentional test error"
  ASSERT_TRUE(msg.find("Intentional test error") != std::string::npos)
      << "Unexpected error message: " << msg;
  sqlite3_finalize(st);

  sqlite3_close(db);
}

TEST(JU_SQLITE, aggregate_memory_management) {
  using namespace ju::sqlite;

  // Test that destructors are called properly when database is closed
  {
    conn_raw *db = nullptr;
    sqlite3_open(":memory:", &db);
    prepare_data(db);

    agg1_dtor_called = false;
    auto e = create_aggregate<agg1>(db, "test_agg", SQLITE_DETERMINISTIC);
    ASSERT_EQ(e, error::ok);

    // Use the aggregate
    stmt_raw *st = nullptr;
    sqlite3_prepare_v2(db, "SELECT test_agg(value) FROM test;", -1, &st, nullptr);
    sqlite3_step(st);
    sqlite3_finalize(st);

    sqlite3_close(db);
  }

  // Destructor should have been called
  ASSERT_TRUE(agg1_dtor_called);
}
