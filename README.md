# [WIP] SQLite3 header only wrapper

## Highlights

- Safe, RAII, zero-cost wrapper for SQLite3 C API
- Avoid throwing, returning error codes as SQLite3 does
- Supports SQLite3 pointer API
- Easy and safe to create UDF and UDAF
  - Your function / functor / lambda just works
  - Your aggregate just works (define step, (inverse), and value member functiions)
- Integrate user-defined types using tag dispatch. Simply dispatch ju::sqlite::cast_tag (SQLite -> C++) and ju::sqlite::bind_tag (C++ -> SQLite)
