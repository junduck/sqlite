#pragma once

#include <memory>
#include <string>

#include "common.hpp"

namespace ju::sqlite {
class [[nodiscard("Connection closed on discard.")]] conn {
  struct deleter_type {
    void operator()(conn_raw *c) const noexcept { sqlite3_close(c); }
  };
  std::unique_ptr<conn_raw, deleter_type> db;

public:
  conn() noexcept = default;
  explicit conn(conn_raw *db) noexcept : db(db) {}

  conn(conn &&other) noexcept = default;
  conn &operator=(conn &&other) noexcept = default;

  explicit operator bool() const noexcept { return !!db; }

  conn_raw *handle() const noexcept { return db.get(); }
  operator conn_raw *() const noexcept { return handle(); }

  conn(std::string const &filename, int flags) noexcept : conn{} {
    // std::outptr when?
    conn_raw *db_raw = nullptr;
    auto rc = sqlite3_open_v2(filename.c_str(), &db_raw, flags, nullptr);
    if (rc == SQLITE_OK) {
      db.reset(db_raw);
    }
  }

  void close() noexcept { db.reset(); }
};

/**
 * @brief Opens a SQLite database connection with the specified filename and flags.
 *
 * This function creates a `conn` object that manages the SQLite database connection.
 * It uses `sqlite3_open_v2` to open the database file with the given flags.
 *
 * @param filename The name of the SQLite database file to open.
 * @param flags    Flags for opening the database (e.g., SQLITE_OPEN_READWRITE).
 * @return        A `conn` object managing the opened database connection.
 */
inline conn open_conn(std::string const &filename, int flags = SQLITE_OPEN_READWRITE) {
  return conn{filename, flags};
}

/**
 * @brief Opens a SQLite in-memory database connection.
 *
 * This function creates a `conn` object that manages an in-memory SQLite database.
 * It uses `sqlite3_open_v2` with the ":memory:" filename and appropriate flags to create
 * a new in-memory database.
 *
 * @return A `conn` object managing the in-memory database connection.
 */
inline conn open_conn_memory() noexcept {
  return conn{":memory:", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE};
}
} // namespace ju::sqlite
