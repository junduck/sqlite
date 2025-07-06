#pragma once

#include <string>

#include "common.hpp"
#include "error.hpp"

namespace ju::sqlite {

enum class transaction_mode : int {
  deferred,
  immediate,
  exclusive,
};

class [[nodiscard("Transaction rolled back on discard.")]] transaction {
  conn_raw *db;
  bool active;

public:
  transaction(conn_raw *conn, transaction_mode mode = transaction_mode::deferred)
      : db(conn), active(true) {
    if (db) {
      char const *sql = nullptr;
      switch (mode) {
      case transaction_mode::deferred:
        sql = "BEGIN DEFERRED TRANSACTION";
        break;
      case transaction_mode::immediate:
        sql = "BEGIN IMMEDIATE TRANSACTION";
        break;
      case transaction_mode::exclusive:
        sql = "BEGIN EXCLUSIVE TRANSACTION";
        break;
      }
      int rc = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
      if (rc != SQLITE_OK)
        db = nullptr;
    }
  }

  ~transaction() {
    if (!is_active())
      return; // No active transaction to rollback

    sqlite3_exec(db, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
  }

  explicit operator bool() const { return db != nullptr; }

  /**
   * @brief Saves a savepoint with the given name in the current transaction.
   *
   * This function creates a savepoint in the current transaction, allowing you to
   * roll back to this point later if needed. The savepoint can be released or rolled
   * back to using the `release` or `rollback` methods, respectively.
   *
   * @param save The name of the savepoint to create.
   * @return An `error` indicating the result of the savepoint creation operation.
   */
  error savepoint(std::string const &save) const noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    std::string sql = "SAVEPOINT " + save;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    return to_error(rc);
  }

  /**
   * @brief Releases a savepoint with the given name in the current transaction.
   *
   * This function releases a previously created savepoint, allowing the transaction to
   * continue without being able to roll back to that savepoint anymore.
   *
   * @param save The name of the savepoint to release.
   * @return An `error` indicating the result of the release operation.
   */
  error release(std::string const &save) const noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    std::string sql = "RELEASE SAVEPOINT " + save;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    return to_error(rc);
  }

  /**
   * @brief Rolls back to a savepoint with the given name in the current transaction.
   *
   * This function rolls back the transaction to the specified savepoint, undoing any
   * changes made after that savepoint was created. The savepoint remains active after
   * the rollback, allowing further operations on it.
   *
   * @param save The name of the savepoint to roll back to.
   * @return An `error` indicating the result of the rollback operation.
   */
  error rollback(std::string const &save) const noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    std::string sql = "ROLLBACK TO SAVEPOINT " + save;
    auto rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    return to_error(rc);
  }

  /**
   * @brief Rolls back the current transaction.
   *
   * This function rolls back the current transaction, undoing all changes made since
   * the transaction began. After a rollback, the transaction is marked as inactive.
   *
   * @return An `error` indicating the result of the rollback operation.
   */
  [[nodiscard]] error rollback() noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    auto rc = sqlite3_exec(db, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
    active = false; // Mark transaction as inactive, ROLLBACK TRANSACTION never fails
    return to_error(rc);
  }

  /**
   * @brief Commits the current transaction.
   *
   * This function commits the current transaction, making all changes made since the
   * transaction began permanent. After a commit, the transaction is marked as inactive.
   *
   * @return An `error` indicating the result of the commit operation.
   */
  [[nodiscard]] error commit() noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    auto rc = sqlite3_exec(db, "COMMIT TRANSACTION", nullptr, nullptr, nullptr);
    if (rc == err::ok)
      active = false; // Mark transaction as inactive only if commit is successful
    return to_error(rc);
  }

  /**
   * @brief Checks if the transaction is currently active.
   *
   * This function returns true if the transaction is active, meaning it has been
   * successfully started and has not yet been committed or rolled back.
   *
   * @return True if the transaction is active, false otherwise.
   */
  [[nodiscard]] bool is_active() const noexcept { return db && active; }
};

/**
 * @brief Begins a new transaction on the given SQLite database connection.
 *
 * This function creates a `transaction` object that manages the lifecycle of a
 * transaction on the specified database connection. The transaction will be automatically
 * rolled back if the `transaction` object is discarded without committing.
 *
 * @param db Pointer to the raw SQLite database connection.
 * @param mode The mode of the transaction (deferred, immediate, or exclusive).
 * @return A `transaction` object managing the transaction.
 */
inline transaction begin_transaction(conn_raw *db,
                                     transaction_mode mode = transaction_mode::deferred) {
  return transaction{db, mode};
}
} // namespace ju::sqlite
