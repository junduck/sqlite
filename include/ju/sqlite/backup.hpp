#pragma once

#include <memory>
#include <string>

#include "common.hpp"
#include "error.hpp"

namespace ju::sqlite {
class [[nodiscard("Backup finalised on discard.")]] backup {
  struct deleter_type {
    void operator()(backup_raw *b) const noexcept { sqlite3_backup_finish(b); }
  };
  std::unique_ptr<backup_raw, deleter_type> bak;

public:
  backup() noexcept = default;
  backup(backup_raw *b) noexcept : bak(b) {}
  backup(backup &&other) noexcept = default;
  backup &operator=(backup &&other) noexcept = default;

  explicit operator bool() const noexcept { return !!bak; }

  backup_raw *handle() const noexcept { return bak.get(); }

  error step(int pages = -1) const noexcept {
    auto rc = sqlite3_backup_step(handle(), pages);
    return to_error(rc);
  }

  error finish() noexcept {
    if (!bak) {
      return error::ok; // No backup to finish
    }
    auto ptr = bak.release();
    auto rc = sqlite3_backup_finish(ptr);
    return to_error(rc);
  }
};

/**
 * @brief Prepares a SQLite backup object for copying data from a source database to a
 * destination database.
 *
 * This function initializes a backup operation from the specified source database and
 * source database name to the specified destination database and destination database
 * name. It wraps the raw SQLite backup handle in a RAII-style `backup` object for safe
 * resource management.
 *
 * @param dest Pointer to the destination SQLite database connection.
 * @param dest_name Name of the destination database (e.g., "main", "temp").
 * @param source Pointer to the source SQLite database connection.
 * @param source_name Name of the source database (e.g., "main", "temp").
 * @return A `backup` object managing the backup operation. If initialization fails,
 * returns an empty `backup`.
 */
inline backup prepare_backup(conn_raw *dest,
                             const std::string &dest_name,
                             conn_raw *source,
                             const std::string &source_name) {
  backup_raw *raw_backup =
      sqlite3_backup_init(dest, dest_name.c_str(), source, source_name.c_str());
  if (!raw_backup) {
    return {}; // Backup initialization failed
  }
  return backup{raw_backup};
}

/**
 * @brief Prepares a SQLite backup object for copying data from the main database of a
 * source connection to the main database of a destination connection.
 *
 * This function is a convenience wrapper around `prepare_backup` that uses "main" as
 * both the destination and source database names.
 *
 * @param dest Pointer to the destination SQLite database connection.
 * @param source Pointer to the source SQLite database connection.
 * @return A `backup` object managing the backup operation. If initialization fails,
 * returns an empty `backup`.
 */
inline backup prepare_backup_main(conn_raw *dest, conn_raw *source) {
  return prepare_backup(dest, "main", source, "main");
}
} // namespace ju::sqlite
