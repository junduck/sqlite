#pragma once

#include "sqlite3.h"

namespace ju::sqlite {
namespace err {
constexpr inline int abort = SQLITE_ABORT;
constexpr inline int auth = SQLITE_AUTH;
constexpr inline int busy = SQLITE_BUSY;
constexpr inline int cantopen = SQLITE_CANTOPEN;
constexpr inline int constraint = SQLITE_CONSTRAINT;
constexpr inline int corrupt = SQLITE_CORRUPT;
constexpr inline int done = SQLITE_DONE; // not an error
constexpr inline int empty = SQLITE_EMPTY;
constexpr inline int error = SQLITE_ERROR;
constexpr inline int format = SQLITE_FORMAT;
constexpr inline int full = SQLITE_FULL;
constexpr inline int internal = SQLITE_INTERNAL;
constexpr inline int interrupt = SQLITE_INTERRUPT;
constexpr inline int ioerr = SQLITE_IOERR;
constexpr inline int locked = SQLITE_LOCKED;
constexpr inline int mismatch = SQLITE_MISMATCH;
constexpr inline int misuse = SQLITE_MISUSE;
constexpr inline int nolfs = SQLITE_NOLFS;
constexpr inline int nomem = SQLITE_NOMEM;
constexpr inline int notadb = SQLITE_NOTADB;
constexpr inline int notfound = SQLITE_NOTFOUND;
constexpr inline int notice = SQLITE_NOTICE;
constexpr inline int ok = SQLITE_OK; // not an error
constexpr inline int perm = SQLITE_PERM;
constexpr inline int protocol = SQLITE_PROTOCOL;
constexpr inline int range = SQLITE_RANGE;
constexpr inline int readonly = SQLITE_READONLY;
constexpr inline int row = SQLITE_ROW; // not an error
constexpr inline int schema = SQLITE_SCHEMA;
constexpr inline int toobig = SQLITE_TOOBIG;
constexpr inline int warning = SQLITE_WARNING;
// extended
constexpr inline int abort_rollback = SQLITE_ABORT_ROLLBACK;
constexpr inline int auth_user = SQLITE_AUTH_USER;
constexpr inline int busy_recovery = SQLITE_BUSY_RECOVERY;
constexpr inline int busy_snapshot = SQLITE_BUSY_SNAPSHOT;
constexpr inline int busy_timeout = SQLITE_BUSY_TIMEOUT;
constexpr inline int cantopen_convpath = SQLITE_CANTOPEN_CONVPATH;
constexpr inline int cantopen_dirtywal = SQLITE_CANTOPEN_DIRTYWAL;
constexpr inline int cantopen_fullpath = SQLITE_CANTOPEN_FULLPATH;
constexpr inline int cantopen_isdir = SQLITE_CANTOPEN_ISDIR;
constexpr inline int cantopen_notempdir = SQLITE_CANTOPEN_NOTEMPDIR;
constexpr inline int cantopen_symlink = SQLITE_CANTOPEN_SYMLINK;
constexpr inline int constraint_check = SQLITE_CONSTRAINT_CHECK;
constexpr inline int constraint_commit_hook = SQLITE_CONSTRAINT_COMMITHOOK;
constexpr inline int constraint_datatype = SQLITE_CONSTRAINT_DATATYPE;
constexpr inline int constraint_foreignkey = SQLITE_CONSTRAINT_FOREIGNKEY;
constexpr inline int constraint_function = SQLITE_CONSTRAINT_FUNCTION;
constexpr inline int constraint_notnull = SQLITE_CONSTRAINT_NOTNULL;
constexpr inline int constraint_pinned = SQLITE_CONSTRAINT_PINNED;
constexpr inline int constraint_primarykey = SQLITE_CONSTRAINT_PRIMARYKEY;
constexpr inline int constraint_rowid = SQLITE_CONSTRAINT_ROWID;
constexpr inline int constraint_trigger = SQLITE_CONSTRAINT_TRIGGER;
constexpr inline int constraint_unique = SQLITE_CONSTRAINT_UNIQUE;
constexpr inline int constraint_vtab = SQLITE_CONSTRAINT_VTAB;
constexpr inline int corrupt_index = SQLITE_CORRUPT_INDEX;
constexpr inline int corrupt_sequence = SQLITE_CORRUPT_SEQUENCE;
constexpr inline int corrupt_vtab = SQLITE_CORRUPT_VTAB;
constexpr inline int error_missing_collseq = SQLITE_ERROR_MISSING_COLLSEQ;
constexpr inline int error_retry = SQLITE_ERROR_RETRY;
constexpr inline int error_snapshot = SQLITE_ERROR_SNAPSHOT;
constexpr inline int ioerr_access = SQLITE_IOERR_ACCESS;
constexpr inline int ioerr_auth = SQLITE_IOERR_AUTH;
constexpr inline int ioerr_begin_atomic = SQLITE_IOERR_BEGIN_ATOMIC;
constexpr inline int ioerr_blocked = SQLITE_IOERR_BLOCKED;
constexpr inline int ioerr_checkreservedlock = SQLITE_IOERR_CHECKRESERVEDLOCK;
constexpr inline int ioerr_close = SQLITE_IOERR_CLOSE;
constexpr inline int ioerr_commit_atomic = SQLITE_IOERR_COMMIT_ATOMIC;
constexpr inline int ioerr_convpath = SQLITE_IOERR_CONVPATH;
constexpr inline int ioerr_corruptfs = SQLITE_IOERR_CORRUPTFS;
constexpr inline int ioerr_data = SQLITE_IOERR_DATA;
constexpr inline int ioerr_delete = SQLITE_IOERR_DELETE;
constexpr inline int ioerr_delete_noent = SQLITE_IOERR_DELETE_NOENT;
constexpr inline int ioerr_dir_close = SQLITE_IOERR_DIR_CLOSE;
constexpr inline int ioerr_dir_fsync = SQLITE_IOERR_DIR_FSYNC;
constexpr inline int ioerr_fstat = SQLITE_IOERR_FSTAT;
constexpr inline int ioerr_fsync = SQLITE_IOERR_FSYNC;
constexpr inline int ioerr_gettemppath = SQLITE_IOERR_GETTEMPPATH;
constexpr inline int ioerr_lock = SQLITE_IOERR_LOCK;
constexpr inline int ioerr_mmap = SQLITE_IOERR_MMAP;
constexpr inline int ioerr_nomem = SQLITE_IOERR_NOMEM;
constexpr inline int ioerr_rdlock = SQLITE_IOERR_RDLOCK;
constexpr inline int ioerr_read = SQLITE_IOERR_READ;
constexpr inline int ioerr_rollback_atomic = SQLITE_IOERR_ROLLBACK_ATOMIC;
constexpr inline int ioerr_seek = SQLITE_IOERR_SEEK;
constexpr inline int ioerr_shmlock = SQLITE_IOERR_SHMLOCK;
constexpr inline int ioerr_shmmap = SQLITE_IOERR_SHMMAP;
constexpr inline int ioerr_shmopen = SQLITE_IOERR_SHMOPEN;
constexpr inline int ioerr_shmsize = SQLITE_IOERR_SHMSIZE;
constexpr inline int ioerr_short_read = SQLITE_IOERR_SHORT_READ;
constexpr inline int ioerr_truncate = SQLITE_IOERR_TRUNCATE;
constexpr inline int ioerr_unlock = SQLITE_IOERR_UNLOCK;
constexpr inline int ioerr_vnode = SQLITE_IOERR_VNODE;
constexpr inline int ioerr_write = SQLITE_IOERR_WRITE;
constexpr inline int locked_sharedcache = SQLITE_LOCKED_SHAREDCACHE;
constexpr inline int locked_vtab = SQLITE_LOCKED_VTAB;
constexpr inline int notice_recover_rollback = SQLITE_NOTICE_RECOVER_ROLLBACK;
constexpr inline int notice_recover_wal = SQLITE_NOTICE_RECOVER_WAL;
constexpr inline int ok_load_permanently = SQLITE_OK_LOAD_PERMANENTLY;
constexpr inline int readonly_cantinit = SQLITE_READONLY_CANTINIT;
constexpr inline int readonly_cantlock = SQLITE_READONLY_CANTLOCK;
constexpr inline int readonly_dbmoved = SQLITE_READONLY_DBMOVED;
constexpr inline int readonly_directory = SQLITE_READONLY_DIRECTORY;
constexpr inline int readonly_recovery = SQLITE_READONLY_RECOVERY;
constexpr inline int readonly_rollback = SQLITE_READONLY_ROLLBACK;
constexpr inline int warning_autoindex = SQLITE_WARNING_AUTOINDEX;
} // namespace err

enum class error : int {
  // primary
  abort = SQLITE_ABORT,
  auth = SQLITE_AUTH,
  busy = SQLITE_BUSY,
  cantopen = SQLITE_CANTOPEN,
  constraint = SQLITE_CONSTRAINT,
  corrupt = SQLITE_CORRUPT,
  done = SQLITE_DONE, // not an error
  empty = SQLITE_EMPTY,
  error = SQLITE_ERROR,
  format = SQLITE_FORMAT,
  full = SQLITE_FULL,
  internal = SQLITE_INTERNAL,
  interrupt = SQLITE_INTERRUPT,
  ioerr = SQLITE_IOERR,
  locked = SQLITE_LOCKED,
  mismatch = SQLITE_MISMATCH,
  misuse = SQLITE_MISUSE,
  nolfs = SQLITE_NOLFS,
  nomem = SQLITE_NOMEM,
  notadb = SQLITE_NOTADB,
  notfound = SQLITE_NOTFOUND,
  notice = SQLITE_NOTICE,
  ok = SQLITE_OK, // not an error
  perm = SQLITE_PERM,
  protocol = SQLITE_PROTOCOL,
  range = SQLITE_RANGE,
  readonly = SQLITE_READONLY,
  row = SQLITE_ROW, // not an error
  schema = SQLITE_SCHEMA,
  toobig = SQLITE_TOOBIG,
  warning = SQLITE_WARNING,

  // extended
  abort_rollback = SQLITE_ABORT_ROLLBACK,

  auth_user = SQLITE_AUTH_USER,

  busy_recovery = SQLITE_BUSY_RECOVERY,
  busy_snapshot = SQLITE_BUSY_SNAPSHOT,
  busy_timeout = SQLITE_BUSY_TIMEOUT,

  cantopen_convpath = SQLITE_CANTOPEN_CONVPATH,
  cantopen_dirtywal = SQLITE_CANTOPEN_DIRTYWAL,
  cantopen_fullpath = SQLITE_CANTOPEN_FULLPATH,
  cantopen_isdir = SQLITE_CANTOPEN_ISDIR,
  cantopen_notempdir = SQLITE_CANTOPEN_NOTEMPDIR,
  cantopen_symlink = SQLITE_CANTOPEN_SYMLINK,

  constraint_check = SQLITE_CONSTRAINT_CHECK,
  constraint_commit_hook = SQLITE_CONSTRAINT_COMMITHOOK,
  constraint_datatype = SQLITE_CONSTRAINT_DATATYPE,
  constraint_foreignkey = SQLITE_CONSTRAINT_FOREIGNKEY,
  constraint_function = SQLITE_CONSTRAINT_FUNCTION,
  constraint_notnull = SQLITE_CONSTRAINT_NOTNULL,
  constraint_pinned = SQLITE_CONSTRAINT_PINNED,
  constraint_primarykey = SQLITE_CONSTRAINT_PRIMARYKEY,
  constraint_rowid = SQLITE_CONSTRAINT_ROWID,
  constraint_trigger = SQLITE_CONSTRAINT_TRIGGER,
  constraint_unique = SQLITE_CONSTRAINT_UNIQUE,
  constraint_vtab = SQLITE_CONSTRAINT_VTAB,

  corrupt_index = SQLITE_CORRUPT_INDEX,
  corrupt_sequence = SQLITE_CORRUPT_SEQUENCE,
  corrupt_vtab = SQLITE_CORRUPT_VTAB,

  error_missing_collseq = SQLITE_ERROR_MISSING_COLLSEQ,
  error_retry = SQLITE_ERROR_RETRY,
  error_snapshot = SQLITE_ERROR_SNAPSHOT,

  ioerr_access = SQLITE_IOERR_ACCESS,
  ioerr_auth = SQLITE_IOERR_AUTH,
  ioerr_begin_atomic = SQLITE_IOERR_BEGIN_ATOMIC,
  ioerr_blocked = SQLITE_IOERR_BLOCKED,
  ioerr_checkreservedlock = SQLITE_IOERR_CHECKRESERVEDLOCK,
  ioerr_close = SQLITE_IOERR_CLOSE,
  ioerr_commit_atomic = SQLITE_IOERR_COMMIT_ATOMIC,
  ioerr_convpath = SQLITE_IOERR_CONVPATH,
  ioerr_corruptfs = SQLITE_IOERR_CORRUPTFS,
  ioerr_data = SQLITE_IOERR_DATA,
  ioerr_delete = SQLITE_IOERR_DELETE,
  ioerr_delete_noent = SQLITE_IOERR_DELETE_NOENT,
  ioerr_dir_close = SQLITE_IOERR_DIR_CLOSE,
  ioerr_dir_fsync = SQLITE_IOERR_DIR_FSYNC,
  ioerr_fstat = SQLITE_IOERR_FSTAT,
  ioerr_fsync = SQLITE_IOERR_FSYNC,
  ioerr_gettemppath = SQLITE_IOERR_GETTEMPPATH,
  ioerr_lock = SQLITE_IOERR_LOCK,
  ioerr_mmap = SQLITE_IOERR_MMAP,
  ioerr_nomem = SQLITE_IOERR_NOMEM,
  ioerr_rdlock = SQLITE_IOERR_RDLOCK,
  ioerr_read = SQLITE_IOERR_READ,
  ioerr_rollback_atomic = SQLITE_IOERR_ROLLBACK_ATOMIC,
  ioerr_seek = SQLITE_IOERR_SEEK,
  ioerr_shmlock = SQLITE_IOERR_SHMLOCK,
  ioerr_shmmap = SQLITE_IOERR_SHMMAP,
  ioerr_shmopen = SQLITE_IOERR_SHMOPEN,
  ioerr_shmsize = SQLITE_IOERR_SHMSIZE,
  ioerr_short_read = SQLITE_IOERR_SHORT_READ,
  ioerr_truncate = SQLITE_IOERR_TRUNCATE,
  ioerr_unlock = SQLITE_IOERR_UNLOCK,
  ioerr_vnode = SQLITE_IOERR_VNODE,
  ioerr_write = SQLITE_IOERR_WRITE,

  locked_sharedcache = SQLITE_LOCKED_SHAREDCACHE,
  locked_vtab = SQLITE_LOCKED_VTAB,

  notice_recover_rollback = SQLITE_NOTICE_RECOVER_ROLLBACK,
  notice_recover_wal = SQLITE_NOTICE_RECOVER_WAL,

  ok_load_permanently = SQLITE_OK_LOAD_PERMANENTLY,

  readonly_cantinit = SQLITE_READONLY_CANTINIT,
  readonly_cantlock = SQLITE_READONLY_CANTLOCK,
  readonly_dbmoved = SQLITE_READONLY_DBMOVED,
  readonly_directory = SQLITE_READONLY_DIRECTORY,
  readonly_recovery = SQLITE_READONLY_RECOVERY,
  readonly_rollback = SQLITE_READONLY_ROLLBACK,

  warning_autoindex = SQLITE_WARNING_AUTOINDEX,
};

constexpr inline error to_error(int code) noexcept { return static_cast<error>(code); }
constexpr inline bool is_ok(error e) noexcept {
  return e == error::ok || e == error::row || e == error::done;
}
constexpr inline bool is_error(error e) noexcept {
  return e != error::ok && e != error::row && e != error::done;
}
constexpr inline bool is_row(error e) noexcept { return e == error::row; }
constexpr inline bool is_done(error e) noexcept { return e == error::done; }

} // namespace ju::sqlite
