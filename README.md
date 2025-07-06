# [WIP] SQLite3 header only wrapper

A modern C++20 header-only wrapper for SQLite3 that provides a safe, RAII-based interface with zero-cost abstractions.

## Highlights

- Safe, RAII, zero-cost wrapper for SQLite3 C API
- Avoid throwing, returning error codes as SQLite3 does
- Supports SQLite3 pointer API
- Easy and safe to create UDF and UDAF
  - Your function / functor / lambda just works
  - Your aggregate just works (define step, (inverse), and value member functions)
- Integrate user-defined types using tag dispatch. Simply dispatch `ju::sqlite::cast_tag` (SQLite -> C++) and `ju::sqlite::bind_tag` (C++ -> SQLite)
- Modern C++20 features: ranges support, concepts, and more

## Quick Start

### Basic Database Operations

```cpp
#include "ju/sqlite.hpp"
#include <iostream>

int main() {
    using namespace ju::sqlite;

    // Open database connection
    auto db = open_conn("example.db", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    if (!db) {
        std::cerr << "Failed to open database\n";
        return 1;
    }

    // Create table
    auto create_stmt = prepare_stmt(db, R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            age INTEGER,
            score REAL
        )
    )");

    if (auto rc = create_stmt.exec(); rc != error::ok) {
        std::cerr << "Failed to create table\n";
        return 1;
    }

    return 0;
}
```

### Inserting Data with Parameter Binding

```cpp
// Insert data using parameter binding
auto insert_stmt = prepare_stmt(db, "INSERT INTO users (name, age, score) VALUES (?, ?, ?)");

// Bind parameters and execute
auto rc = insert_stmt.bind_exec("Alice", 25, 95.5);
if (rc != error::ok) {
    std::cerr << "Insert failed\n";
}
```

### Querying Data

```cpp
// Query data with range-based iteration
auto select_stmt = prepare_stmt(db, "SELECT name, age, score FROM users WHERE age > ?");
select_stmt.bind(20);

for (auto row : select_stmt) {
    // Extract columns by type
    auto [name, age, score] = row.get<std::string, int, double>();
    std::cout << "Name: " << name << ", Age: " << age << ", Score: " << score << "\n";

    // Or extract individual columns
    std::string name2 = row.get<std::string>(0);
    int age2 = row.get<int>(1);
}
```

### User-Defined Functions (UDF)

```cpp
// Register a simple function
int add_one(int x) {
    return x + 1;
}

register_function(db, "add_one", SQLITE_DETERMINISTIC, &add_one);

// Use lambda functions
register_function(db, "multiply", SQLITE_DETERMINISTIC, [](int a, int b) {
    return a * b;
});

// Use function objects
struct StringLength {
    int operator()(std::string const& str) {
        return static_cast<int>(str.length());
    }
};

create_function<StringLength>(db, "str_len", SQLITE_DETERMINISTIC);

// Now use in SQL
auto stmt = prepare_stmt(db, "SELECT add_one(5), multiply(3, 4), str_len('hello')");
```

### User-Defined Aggregate Functions (UDAF)

```cpp
// Simple sum aggregate
struct SumAggregate {
    int total = 0;

    void step(int value) {
        total += value;
    }

    int value() const {
        return total;
    }
};

create_aggregate<SumAggregate>(db, "my_sum");

// Windowed aggregate with inverse function
struct MovingAverage {
    std::vector<double> values;
    double sum = 0.0;

    void step(double value) {
        values.push_back(value);
        sum += value;
    }

    void inverse(double value) {
        auto it = std::find(values.begin(), values.end(), value);
        if (it != values.end()) {
            values.erase(it);
            sum -= value;
        }
    }

    double value() const {
        return values.empty() ? 0.0 : sum / values.size();
    }
};

create_window_aggregate<MovingAverage>(db, "moving_avg");

// Use in SQL
auto stmt = prepare_stmt(db, "SELECT my_sum(score), moving_avg(score) OVER (ROWS 3 PRECEDING) FROM users");
```

### Custom Type Integration

```cpp
// Define custom type
struct Point {
  double x, y;

  // Implement binding (C++ -> SQLite) statement
  friend auto tag_invoke(tag::bind_tag, stmt_raw *st, int idx, Point const& p) {
    std::string serialized = std::to_string(p.x) + "," + std::to_string(p.y);
    return to_error(sqlite3_bind_text(st, idx, serialized.c_str(), -1, SQLITE_TRANSIENT));
  }

  // Implement binding (C++ -> SQLite) context (UDF)
  friend auto tag_invoke(tag::bind_tag, context_raw* ctx, Point const& p) {
    std::string serialized = std::to_string(p.x) + "," + std::to_string(p.y);
    sqlite3_result_text(ctx, serialized.c_str(), -1, SQLITE_TRANSIENT);
  }

  // Implement casting (SQLite -> C++)
  friend auto tag_invoke(tag::cast_tag, type_t<Point>, value_raw *val) {
    auto const* p = reinterpret_cast<char const*>(sqlite3_value_text(val));
    std::string_view data{p, sqlite3_value_bytes(val)};
    size_t comma = data.find(',');
    return Point{std::stod(data.substr(0, comma)), std::stod(data.substr(comma + 1))};
  }
};

// Now use Point directly
auto insert_stmt = prepare_stmt(db, "INSERT INTO points (location) VALUES (?)");
Point p{3.14, 2.71};
insert_stmt.bind_exec(p);
```

### Transaction Management

```cpp
// RAII transaction
{
    auto txn = transaction(db);

    // Perform multiple operations
    auto stmt1 = prepare_stmt(db, "INSERT INTO users (name) VALUES (?)");
    stmt1.bind_exec("Charlie");

    auto stmt2 = prepare_stmt(db, "UPDATE users SET age = ? WHERE name = ?");
    stmt2.bind_exec(28, "Charlie");

    // Transaction rolls back automatically when txn goes out of scope
    // Call txn.commit() to explicitly commit changes
}
```

### Error Handling

```cpp
// All operations return error codes, no exceptions
auto stmt = prepare_stmt(db, "SELECT * FROM users");
if (!stmt) {
    std::cerr << "Failed to prepare statement\n";
    return;
}

auto rc = stmt.exec();
if (rc != error::ok) {
    std::cerr << "Execution failed with error: " << static_cast<int>(rc) << "\n";
}

// Check iterator state
for (auto it = stmt.begin(); it != stmt.end(); ++it) {
    if (it.state() != error::ok) {
        std::cerr << "Iterator error\n";
        break;
    }
    // Process row...
}
```

## Building

This is a header-only library. Simply include the main header:

```cpp
#include "ju/sqlite.hpp"
```

Make sure to link against SQLite3:

```bash
g++ -std=c++20 your_program.cpp -lsqlite3
```

## Requirements

- C++20 compliant compiler
- SQLite3 library

## TODO

- [ ] Add runnable examples
- [ ] Better documentation
- [ ] Virtual table support

## Credits

README is mostly generated by Copilot, with some manual adjustments.
