#include "sqlite3/sqlite3.h"

#include <cstdint>
#include <cstring>
#include <iostream>

// Helper: Read a "varint" (up to 9 bytes, SQLite's format)
int64_t readVarint(const unsigned char *p, uint64_t *v) {
  int i = 0;
  *v = 0;
  while (i < 9) {
    *v = (*v << 7) | (p[i] & 0x7F);
    if (!(p[i] & 0x80)) {
      return i + 1;
    }
    i++;
  }
  // 9th byte is 8 bits
  *v = (*v << 8) | p[8];
  return 9;
}

// Decode one serial type and content
void decode_serial(const unsigned char *content,
                   int serial_type,
                   void *out,
                   int *out_len) {
  switch (serial_type) {
  case 0: // NULL
    *(char *)out = 0;
    *out_len = 0;
    break;
  case 1: // int8
    *(int64_t *)out = (int8_t)content[0];
    *out_len = 1;
    break;
  case 2: // int16
    *(int64_t *)out = (int16_t)((content[0] << 8) | content[1]);
    *out_len = 2;
    break;
  case 3: // int24
    *(int64_t *)out = (int32_t)((content[0] << 16) | (content[1] << 8) | content[2]);
    *out_len = 3;
    break;
  case 4: // int32
    *(int64_t *)out = (int32_t)((content[0] << 24) | (content[1] << 16) |
                                (content[2] << 8) | content[3]);
    *out_len = 4;
    break;
  case 5: // int48
    *(int64_t *)out = ((int64_t)content[0] << 40) | ((int64_t)content[1] << 32) |
                      (content[2] << 24) | (content[3] << 16) | (content[4] << 8) |
                      content[5];
    *out_len = 6;
    break;
  case 6: // int64
    *(int64_t *)out = ((int64_t)content[0] << 56) | ((int64_t)content[1] << 48) |
                      ((int64_t)content[2] << 40) | ((int64_t)content[3] << 32) |
                      ((int64_t)content[4] << 24) | ((int64_t)content[5] << 16) |
                      ((int64_t)content[6] << 8) | content[7];
    *out_len = 8;
    break;
  case 7: // IEEE float64 (not handled here)
    *out_len = 8;
    break;
  default:
    if (serial_type >= 13 && (serial_type % 2) == 1) { // TEXT
      int len = (serial_type - 13) / 2;
      memcpy(out, content, static_cast<size_t>(len));
      ((char *)out)[len] = '\0'; // Null-terminate
      *out_len = len;
    } else if (serial_type >= 12 && (serial_type % 2) == 0) { // BLOB
      int len = (serial_type - 12) / 2;
      memcpy(out, content, static_cast<size_t>(len));
      *out_len = len;
    } else {
      *out_len = 0; // Not handled
    }
    break;
  }
}

// Main decode function for a payload with N columns
void decode_payload(const unsigned char *buf, int) {
  int64_t i, pos = 0;
  uint64_t header_size;
  pos += readVarint(buf, &header_size);

  // Read serial types
  int nCol = 0;
  uint64_t serial_types[32]; // support up to 32 columns
  uint64_t header_pos = static_cast<uint64_t>(pos);
  while (header_pos < header_size) {
    header_pos +=
        static_cast<uint64_t>(readVarint(buf + header_pos, &serial_types[nCol]));
    nCol++;
  }

  // Read column values
  int64_t content_pos = static_cast<int64_t>(header_size);
  for (i = 0; i < nCol; i++) {
    char str[100];
    int64_t ival = 0;
    int len = 0;
    decode_serial(buf + content_pos, (int)serial_types[i], str, &len);
    if (serial_types[i] == 0) {
      std::cout << "col" << (i + 1) << ": NULL " << std::endl;
    } else if (serial_types[i] >= 13 && (serial_types[i] % 2) == 1) {
      std::cout << "col" << (i + 1) << ": TEXT '" << str << "'" << std::endl;
    } else {
      decode_serial(buf + content_pos, (int)serial_types[i], &ival, &len);
      std::cout << "col" << (i + 1) << ": INT " << ival << std::endl;
    }
    content_pos += len;
  }

  std::cout << "Decoded " << nCol << " columns from payload." << std::endl;
}

void hook_fn(void *rec,
             int op,
             const char *db_name,
             const char *table_name,
             sqlite3_int64 row_id) {
  // This function will be called on every update operation
  // op can be SQLITE_INSERT, SQLITE_UPDATE, or SQLITE_DELETE
  // db_name is the name of the database
  // table_name is the name of the table
  // row_id is the ID of the row being updated
  (void)op;         // Unused parameter
  (void)db_name;    // Unused parameter
  (void)table_name; // Unused parameter
  (void)row_id;     // Unused parameter

  auto *payload = reinterpret_cast<sqlite3_exp_simple_record *>(rec);
  decode_payload(static_cast<const unsigned char *>(payload->pData), payload->nData);
}

int main() {

  sqlite3 *db;
  int rc = sqlite3_open(":memory:", &db);
  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return rc;
  }

  sqlite3_update_hook(db, &hook_fn, nullptr);

  // create a table with all data types
  const char *create_table_sql = "CREATE TABLE test ( "
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "name TEXT, "
                                 "age INTEGER, "
                                 "salary REAL, "
                                 "is_active BOOLEAN, "
                                 "data BLOB)";
  rc = sqlite3_exec(db, create_table_sql, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return rc;
  }

  // insert a record
  const char *insert_sql =
      "INSERT INTO test (name, age, salary, is_active, data) "
      "VALUES ('Alice', 30, 50000.0, 1, X'01020304')"; // TODO: fix decode, boolean decode
                                                       // error? serial type 8/9
  rc = sqlite3_exec(db, insert_sql, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return rc;
  }

  insert_sql = "INSERT INTO test (name, age, is_active) "
               "VALUES ('Bob', 25, 0)"; // NULL salary and data
  rc = sqlite3_exec(db, insert_sql, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return rc;
  }

  // update bob
  const char *update_sql = "UPDATE test SET age = 26 WHERE name = 'Bob'";
  rc = sqlite3_exec(db, update_sql, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return rc;
  }

  return 0;
}

/*

Conclusion: we can directly fetch update payload from pData

Observation:

1. Decode is so wrong. Gotta rewrite

2. pData reflects the actual payload binded to the vdbe, not updated full record

3. We should make a new update callback hook:

struct sqlite3_update_record {
  int nData;
  void const* pData; // pointer to the payload data
};

void (update_record_hook_fn*)(void* pApp,
                                 int op,
                                 const char* db_name,
                                 const char* table_name,
                                 sqlite3_int64 row_id,
                                 sqlite3_update_record* rec);

4. It is very possible to avoid roundtrip to database, to implement direct update
pub-sub system.


TODO: we should also extract constraints (e.g. WHERE name = 'Bob')



*/
