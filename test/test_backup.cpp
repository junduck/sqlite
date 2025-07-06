#include "gtest/gtest.h"

#include "ju/sqlite/backup.hpp"
#include <filesystem>
#include <sqlite3.h>
#include <string>

namespace {

// Helper function to open a database
sqlite3 *open_database(const std::string &path) {
  sqlite3 *db = nullptr;
  int rc = sqlite3_open(path.c_str(), &db);
  if (rc != SQLITE_OK) {
    if (db) {
      sqlite3_close(db);
    }
    return nullptr;
  }
  return db;
}

// Helper function to get row count from a table
int get_row_count(sqlite3 *db, const std::string &table_name) {
  std::string sql = "SELECT COUNT(*) FROM " + table_name;
  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return -1;
  }

  int count = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return count;
}

// Helper function to check if table exists
bool table_exists(sqlite3 *db, const std::string &table_name) {
  std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_STATIC);
  bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

  sqlite3_finalize(stmt);
  return exists;
}

} // anonymous namespace

TEST(BackupTest, DefaultConstruction) {
  using namespace ju::sqlite;

  backup bak;
  EXPECT_FALSE(bak); // Should be false for default constructed backup
  EXPECT_EQ(bak.handle(), nullptr);
}

TEST(BackupTest, PrepareBackupWithNonexistentSource) {
  using namespace ju::sqlite;

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  // Try to open a file that shouldn't exist (use readonly mode to prevent creation)
  sqlite3 *source = nullptr;
  int rc = sqlite3_open_v2("nonexistent_file_that_should_not_exist.db",
                           &source,
                           SQLITE_OPEN_READONLY,
                           nullptr);
  EXPECT_NE(rc, SQLITE_OK); // Should fail to open in readonly mode
  if (source) {
    sqlite3_close(source);
    source = nullptr;
  }

  {
    // Test backup with null source
    auto bak = prepare_backup_main(dest, source);
    EXPECT_FALSE(bak) << "Backup with null source should fail";
  }

  sqlite3_close(dest);
}

TEST(BackupTest, PrepareBackupFromIrisDb) {
  using namespace ju::sqlite;

  // Test assumes iris.db is copied to data/iris.db by CMake
  std::string iris_path = "../data/iris.db";

  // Check if file exists and is not empty
  std::filesystem::path file_path(iris_path);
  ASSERT_TRUE(std::filesystem::exists(file_path))
      << "iris.db file does not exist at " << iris_path;
  ASSERT_GT(std::filesystem::file_size(file_path), 0) << "iris.db file is empty";

  sqlite3 *source = open_database(iris_path);
  ASSERT_NE(source, nullptr)
      << "Failed to open iris.db - make sure it exists in data/iris.db";

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  {
    // Test backup preparation with error checking
    sqlite3_backup *raw_backup = sqlite3_backup_init(dest, "main", source, "main");
    EXPECT_NE(raw_backup, nullptr) << "sqlite3_backup_init failed";

    if (raw_backup) {
      // Test our backup wrapper
      backup bak(raw_backup);
      EXPECT_TRUE(bak) << "Failed to create backup wrapper";
      EXPECT_NE(bak.handle(), nullptr);
    }
  }

  sqlite3_close(source);
  sqlite3_close(dest);
}

TEST(BackupTest, BackupStepAndFinish) {
  using namespace ju::sqlite;

  std::string iris_path = "../data/iris.db";

  sqlite3 *source = open_database(iris_path);
  ASSERT_NE(source, nullptr);

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  {
    auto bak = prepare_backup_main(dest, source);
    ASSERT_TRUE(bak);

    // Perform backup step by step
    error err = bak.step(5); // Copy 5 pages at a time
    while (err == error::ok) {
      err = bak.step(5);
    }

    // Should end with done, not an error
    EXPECT_EQ(err, error::done) << "Backup should complete with 'done' status";

    // Manually finish the backup
    err = bak.finish();
    EXPECT_EQ(err, error::ok) << "Backup finish should return ok";
  }

  sqlite3_close(source);
  sqlite3_close(dest);
}

TEST(BackupTest, BackupAllAtOnce) {
  using namespace ju::sqlite;

  std::string iris_path = "../data/iris.db";

  sqlite3 *source = open_database(iris_path);
  ASSERT_NE(source, nullptr);

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  {
    auto bak = prepare_backup_main(dest, source);
    ASSERT_TRUE(bak);

    // Copy all pages at once (-1 means all)
    error err = bak.step(-1);
    EXPECT_EQ(err, error::done) << "Single step backup should complete with 'done'";
  }

  sqlite3_close(source);
  sqlite3_close(dest);
}

TEST(BackupTest, VerifyBackupIntegrity) {
  using namespace ju::sqlite;

  std::string iris_path = "../data/iris.db";

  sqlite3 *source = open_database(iris_path);
  ASSERT_NE(source, nullptr);

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  // Get source table info before backup
  bool source_has_table = table_exists(source, "iris");
  int source_row_count = -1;
  if (source_has_table) {
    source_row_count = get_row_count(source, "iris");
  }

  {
    // Perform complete backup
    auto bak = prepare_backup_main(dest, source);
    ASSERT_TRUE(bak);

    error err = bak.step(-1);
    EXPECT_EQ(err, error::done);
  }

  // Verify destination has the same data
  if (source_has_table) {
    EXPECT_TRUE(table_exists(dest, "iris")) << "Destination should have iris table";
    int dest_row_count = get_row_count(dest, "iris");
    EXPECT_EQ(source_row_count, dest_row_count) << "Row counts should match";
    EXPECT_GT(dest_row_count, 0) << "Should have actual data";
  }

  sqlite3_close(source);
  sqlite3_close(dest);
}

TEST(BackupTest, PrepareBackupWithCustomNames) {
  using namespace ju::sqlite;

  std::string iris_path = "../data/iris.db";

  sqlite3 *source = open_database(iris_path);
  ASSERT_NE(source, nullptr);

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  {
    // Test with custom database names
    auto bak = prepare_backup(dest, "main", source, "main");
    EXPECT_TRUE(bak);

    // Should be equivalent to prepare_backup_main
    auto bak2 = prepare_backup_main(dest, source);
    EXPECT_TRUE(bak2);
  }

  sqlite3_close(source);
  sqlite3_close(dest);
}

TEST(BackupTest, MoveSemantics) {
  using namespace ju::sqlite;

  std::string iris_path = "../data/iris.db";

  sqlite3 *source = open_database(iris_path);
  ASSERT_NE(source, nullptr);

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  {
    auto bak1 = prepare_backup_main(dest, source);
    ASSERT_TRUE(bak1);

    // Test move constructor
    backup bak2 = std::move(bak1);
    EXPECT_TRUE(bak2);
    EXPECT_FALSE(bak1); // Original should be empty after move

    // Test move assignment
    backup bak3;
    EXPECT_FALSE(bak3);
    bak3 = std::move(bak2);
    EXPECT_TRUE(bak3);
    EXPECT_FALSE(bak2); // Should be empty after move
  }

  sqlite3_close(source);
  sqlite3_close(dest);
}

TEST(BackupTest, FinishEmptyBackup) {
  using namespace ju::sqlite;

  backup bak; // Default constructed, empty backup
  EXPECT_FALSE(bak);

  // Should be safe to finish an empty backup
  error err = bak.finish();
  EXPECT_EQ(err, error::ok);
}

TEST(BackupTest, BackupFailsWithInvalidDatabases) {
  using namespace ju::sqlite;

  // Test with null pointers (this should fail gracefully)
  auto bak = prepare_backup_main(nullptr, nullptr);
  EXPECT_FALSE(bak) << "Backup with null databases should fail";
}

TEST(BackupTest, BackupWithAttachedDatabase) {
  using namespace ju::sqlite;

  std::string iris_path = "../data/iris.db";

  sqlite3 *source = open_database(iris_path);
  ASSERT_NE(source, nullptr);

  sqlite3 *dest = open_database(":memory:");
  ASSERT_NE(dest, nullptr);

  // Attach another database to destination
  char *err_msg = nullptr;
  int rc = sqlite3_exec(
      dest, "ATTACH DATABASE ':memory:' AS temp_db", nullptr, nullptr, &err_msg);
  ASSERT_EQ(rc, SQLITE_OK) << "Failed to attach database: "
                           << (err_msg ? err_msg : "unknown error");
  if (err_msg)
    sqlite3_free(err_msg);

  {
    // Try backing up to the attached database
    auto bak = prepare_backup(dest, "temp_db", source, "main");
    EXPECT_TRUE(bak) << "Should be able to backup to attached database";

    if (bak) {
      error err = bak.step(-1);
      EXPECT_EQ(err, error::done);
    }
  }

  sqlite3_close(source);
  sqlite3_close(dest);
}
