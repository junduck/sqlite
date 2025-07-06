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

  conn(std::string const &filename, int flags = SQLITE_OPEN_READWRITE) noexcept : conn{} {
    // std::outptr when?
    conn_raw *db_raw = nullptr;
    auto rc = sqlite3_open_v2(filename.c_str(), &db_raw, flags, nullptr);
    if (rc == SQLITE_OK) {
      db.reset(db_raw);
    }
  }

  void close() noexcept { db.reset(); }
};

inline conn open_conn(std::string const &filename, int flags = SQLITE_OPEN_READWRITE) {
  return conn{filename, flags};
}
} // namespace ju::sqlite
