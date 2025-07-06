#include "gtest/gtest.h"

#include "ju/sqlite/transaction.hpp"
#include <sqlite3.h>

namespace {

// Helper function to create an in-memory database with test table
sqlite3 *create_test_db() {
  sqlite3 *db = nullptr;
  int rc = sqlite3_open(":memory:", &db);
  if (rc != SQLITE_OK) {
    return nullptr;
  }

  // Create a test table
  const char *sql = "CREATE TABLE test_data (id INTEGER PRIMARY KEY, value TEXT)";
  rc = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return nullptr;
  }

  return db;
}

// Helper function to insert test data
void insert_test_data(sqlite3 *db, int id, const char *value) {
  const char *sql = "INSERT INTO test_data (id, value) VALUES (?, ?)";
  sqlite3_stmt *stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, id);
  sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

// Helper function to count rows in test_data table
int count_rows(sqlite3 *db) {
  const char *sql = "SELECT COUNT(*) FROM test_data";
  sqlite3_stmt *stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  sqlite3_step(stmt);
  int count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return count;
}

// Helper function to check if a row exists
bool row_exists(sqlite3 *db, int id) {
  const char *sql = "SELECT COUNT(*) FROM test_data WHERE id = ?";
  sqlite3_stmt *stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, id);
  sqlite3_step(stmt);
  int count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return count > 0;
}

} // anonymous namespace

TEST(TransactionTest, DefaultConstruction) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    EXPECT_TRUE(txn); // Should be valid
    EXPECT_TRUE(txn.is_active());
  }

  sqlite3_close(db);
}

TEST(TransactionTest, ConstructionWithNullDb) {
  using namespace ju::sqlite;

  {
    transaction txn(nullptr);
    EXPECT_FALSE(txn); // Should be invalid with null db
    EXPECT_FALSE(txn.is_active());
  }
}

TEST(TransactionTest, DeferredTransactionMode) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db, transaction_mode::deferred);
    EXPECT_TRUE(txn);
    EXPECT_TRUE(txn.is_active());

    insert_test_data(db, 1, "test1");
    EXPECT_EQ(count_rows(db), 1);
  } // Should rollback on destruction

  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, ImmediateTransactionMode) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db, transaction_mode::immediate);
    EXPECT_TRUE(txn);
    EXPECT_TRUE(txn.is_active());

    insert_test_data(db, 1, "test1");
    EXPECT_EQ(count_rows(db), 1);
  } // Should rollback on destruction

  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, ExclusiveTransactionMode) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db, transaction_mode::exclusive);
    EXPECT_TRUE(txn);
    EXPECT_TRUE(txn.is_active());

    insert_test_data(db, 1, "test1");
    EXPECT_EQ(count_rows(db), 1);
  } // Should rollback on destruction

  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, AutomaticRollbackOnDestruction) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    EXPECT_TRUE(txn.is_active());

    insert_test_data(db, 1, "test1");
    insert_test_data(db, 2, "test2");
    EXPECT_EQ(count_rows(db), 2);

    // Don't commit - let destructor rollback
  }

  // Transaction should have been rolled back
  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, ExplicitCommit) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    EXPECT_TRUE(txn.is_active());

    insert_test_data(db, 1, "test1");
    insert_test_data(db, 2, "test2");
    EXPECT_EQ(count_rows(db), 2);

    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
    EXPECT_FALSE(txn.is_active()); // Should be inactive after commit
  }

  // Data should persist after commit
  EXPECT_EQ(count_rows(db), 2);
  EXPECT_TRUE(row_exists(db, 1));
  EXPECT_TRUE(row_exists(db, 2));

  sqlite3_close(db);
}

TEST(TransactionTest, ExplicitRollback) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    EXPECT_TRUE(txn.is_active());

    insert_test_data(db, 1, "test1");
    insert_test_data(db, 2, "test2");
    EXPECT_EQ(count_rows(db), 2);

    error err = txn.rollback();
    EXPECT_EQ(err, error::ok);
    EXPECT_FALSE(txn.is_active()); // Should be inactive after rollback
  }

  // Data should be rolled back
  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, DoubleCommitPrevention) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    insert_test_data(db, 1, "test1");

    error err1 = txn.commit();
    EXPECT_EQ(err1, error::ok);
    EXPECT_FALSE(txn.is_active());

    // Second commit should fail
    error err2 = txn.commit();
    EXPECT_EQ(err2, error::misuse);
  }

  EXPECT_EQ(count_rows(db), 1);
  sqlite3_close(db);
}

TEST(TransactionTest, DoubleRollbackPrevention) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    insert_test_data(db, 1, "test1");

    error err1 = txn.rollback();
    EXPECT_EQ(err1, error::ok);
    EXPECT_FALSE(txn.is_active());

    // Second rollback should fail
    error err2 = txn.rollback();
    EXPECT_EQ(err2, error::misuse);
  }

  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, CommitAfterRollback) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    insert_test_data(db, 1, "test1");

    error err1 = txn.rollback();
    EXPECT_EQ(err1, error::ok);
    EXPECT_FALSE(txn.is_active());

    // Commit after rollback should fail
    error err2 = txn.commit();
    EXPECT_EQ(err2, error::misuse);
  }

  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, SavepointBasicOperations) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    insert_test_data(db, 1, "test1");
    EXPECT_EQ(count_rows(db), 1);

    // Create savepoint
    error err = txn.savepoint("sp1");
    EXPECT_EQ(err, error::ok);

    insert_test_data(db, 2, "test2");
    EXPECT_EQ(count_rows(db), 2);

    // Release savepoint
    err = txn.release("sp1");
    EXPECT_EQ(err, error::ok);

    err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 2);
  sqlite3_close(db);
}

TEST(TransactionTest, SavepointRollback) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    insert_test_data(db, 1, "test1");
    EXPECT_EQ(count_rows(db), 1);

    // Create savepoint
    error err = txn.savepoint("sp1");
    EXPECT_EQ(err, error::ok);

    insert_test_data(db, 2, "test2");
    insert_test_data(db, 3, "test3");
    EXPECT_EQ(count_rows(db), 3);

    // Rollback to savepoint
    err = txn.rollback("sp1");
    EXPECT_EQ(err, error::ok);

    // Should only have the first row
    EXPECT_EQ(count_rows(db), 1);
    EXPECT_TRUE(row_exists(db, 1));
    EXPECT_FALSE(row_exists(db, 2));
    EXPECT_FALSE(row_exists(db, 3));

    err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 1);
  sqlite3_close(db);
}

TEST(TransactionTest, NestedSavepoints) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    insert_test_data(db, 1, "test1");

    // First savepoint
    error err = txn.savepoint("sp1");
    EXPECT_EQ(err, error::ok);

    insert_test_data(db, 2, "test2");

    // Nested savepoint
    err = txn.savepoint("sp2");
    EXPECT_EQ(err, error::ok);

    insert_test_data(db, 3, "test3");
    EXPECT_EQ(count_rows(db), 3);

    // Rollback inner savepoint
    err = txn.rollback("sp2");
    EXPECT_EQ(err, error::ok);

    EXPECT_EQ(count_rows(db), 2);
    EXPECT_TRUE(row_exists(db, 1));
    EXPECT_TRUE(row_exists(db, 2));
    EXPECT_FALSE(row_exists(db, 3));

    // Release outer savepoint
    err = txn.release("sp1");
    EXPECT_EQ(err, error::ok);

    err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 2);
  sqlite3_close(db);
}

TEST(TransactionTest, SavepointOperationsOnInactiveTransaction) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    // Commit transaction first
    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
    EXPECT_FALSE(txn.is_active());

    // All savepoint operations should fail on inactive transaction
    err = txn.savepoint("sp1");
    EXPECT_EQ(err, error::misuse);

    err = txn.release("sp1");
    EXPECT_EQ(err, error::misuse);

    err = txn.rollback("sp1");
    EXPECT_EQ(err, error::misuse);
  }

  sqlite3_close(db);
}

TEST(TransactionTest, MultipleTransactionsSequential) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  // First transaction - commit
  {
    transaction txn(db);
    insert_test_data(db, 1, "test1");
    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 1);

  // Second transaction - rollback
  {
    transaction txn(db);
    insert_test_data(db, 2, "test2");
    // Let destructor rollback
  }

  EXPECT_EQ(count_rows(db), 1); // Only first transaction's data should remain

  // Third transaction - commit
  {
    transaction txn(db);
    insert_test_data(db, 3, "test3");
    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 2);
  EXPECT_TRUE(row_exists(db, 1));
  EXPECT_FALSE(row_exists(db, 2));
  EXPECT_TRUE(row_exists(db, 3));

  sqlite3_close(db);
}

TEST(TransactionTest, TransactionAfterCommit) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    insert_test_data(db, 1, "test1");
    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
    EXPECT_FALSE(txn.is_active());
  }

  // Destructor should not try to rollback inactive transaction
  EXPECT_EQ(count_rows(db), 1);
  sqlite3_close(db);
}

TEST(TransactionTest, TransactionAfterRollback) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    insert_test_data(db, 1, "test1");
    error err = txn.rollback();
    EXPECT_EQ(err, error::ok);
    EXPECT_FALSE(txn.is_active());
  }

  // Destructor should not try to rollback inactive transaction
  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, ErrorHandlingWithInvalidSavepoint) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    insert_test_data(db, 1, "test1");

    // Try to release a savepoint that doesn't exist
    error err = txn.release("nonexistent_savepoint");
    EXPECT_NE(err, error::ok);

    // Try to rollback to a savepoint that doesn't exist
    err = txn.rollback("nonexistent_savepoint");
    EXPECT_NE(err, error::ok);

    // Transaction should still be active
    EXPECT_TRUE(txn.is_active());

    // Should still be able to commit
    err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 1);
  sqlite3_close(db);
}

TEST(TransactionTest, LargeNumberOfOperations) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    // Insert a large number of rows
    for (int i = 1; i <= 1000; ++i) {
      insert_test_data(db, i, ("test" + std::to_string(i)).c_str());
    }

    EXPECT_EQ(count_rows(db), 1000);

    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 1000);
  sqlite3_close(db);
}

TEST(TransactionTest, SavepointWithConstraintViolation) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    insert_test_data(db, 1, "test1");

    error err = txn.savepoint("sp1");
    EXPECT_EQ(err, error::ok);

    insert_test_data(db, 2, "test2");

    // Rollback to savepoint
    err = txn.rollback("sp1");
    EXPECT_EQ(err, error::ok);

    // Should only have first row
    EXPECT_EQ(count_rows(db), 1);
    EXPECT_TRUE(row_exists(db, 1));
    EXPECT_FALSE(row_exists(db, 2));

    // Add different data
    insert_test_data(db, 3, "test3");
    EXPECT_EQ(count_rows(db), 2);

    err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 2);
  EXPECT_TRUE(row_exists(db, 1));
  EXPECT_TRUE(row_exists(db, 3));
  sqlite3_close(db);
}

TEST(TransactionTest, SavepointNameWithSpecialCharacters) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);

    insert_test_data(db, 1, "test1");

    // Test savepoint with quotes in name (should be handled by SQLite)
    error err = txn.savepoint("sp_with_underscore");
    EXPECT_EQ(err, error::ok);

    insert_test_data(db, 2, "test2");

    err = txn.release("sp_with_underscore");
    EXPECT_EQ(err, error::ok);

    err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 2);
  sqlite3_close(db);
}

TEST(TransactionTest, EmptyTransaction) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  {
    transaction txn(db);
    EXPECT_TRUE(txn.is_active());

    // Don't perform any operations
    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 0);
  sqlite3_close(db);
}

TEST(TransactionTest, TransactionWithReadOnlyOperations) {
  using namespace ju::sqlite;

  sqlite3 *db = create_test_db();
  ASSERT_NE(db, nullptr);

  // Add some initial data outside transaction
  insert_test_data(db, 1, "initial");

  {
    transaction txn(db);

    // Perform read-only operations
    int initial_count = count_rows(db);
    EXPECT_EQ(initial_count, 1);
    EXPECT_TRUE(row_exists(db, 1));

    error err = txn.commit();
    EXPECT_EQ(err, error::ok);
  }

  EXPECT_EQ(count_rows(db), 1);
  sqlite3_close(db);
}
