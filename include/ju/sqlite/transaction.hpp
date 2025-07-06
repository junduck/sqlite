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

class transaction {
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

  error savepoint(std::string const &save) const noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    std::string sql = "SAVEPOINT " + save;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    return to_error(rc);
  }

  error release(std::string const &save) const noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    std::string sql = "RELEASE SAVEPOINT " + save;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    return to_error(rc);
  }

  error rollback(std::string const &save) const noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    std::string sql = "ROLLBACK TO SAVEPOINT " + save;
    auto rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    return to_error(rc);
  }

  error rollback() noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    auto rc = sqlite3_exec(db, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
    active = false; // Mark transaction as inactive, ROLLBACK TRANSACTION never fails
    return to_error(rc);
  }

  error commit() noexcept {
    if (!is_active())
      return error::misuse; // Transaction not active

    auto rc = sqlite3_exec(db, "COMMIT TRANSACTION", nullptr, nullptr, nullptr);
    if (rc == err::ok)
      active = false; // Mark transaction as inactive only if commit is successful
    return to_error(rc);
  }

  bool is_active() const noexcept { return db && active; }
};
} // namespace ju::sqlite
