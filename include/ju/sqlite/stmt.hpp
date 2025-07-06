#pragma once

#include <memory>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>

#include "common.hpp"
#include "tag.hpp"

namespace ju::sqlite {

// row value_raw delegate

enum class value_type : int {
  int_ = SQLITE_INTEGER,
  real = SQLITE_FLOAT,
  text = SQLITE_TEXT,
  blob = SQLITE_BLOB,
  null = SQLITE_NULL,
};

class stmt_row {
  friend class stmt_iterator;

  stmt_raw *st;

  stmt_row() noexcept = default;
  explicit stmt_row(stmt_raw *s) noexcept : st(s) {}

  template <typename... Ts, size_t... Is>
  std::tuple<Ts...> get_impl(std::index_sequence<Is...>) const {
    return std::make_tuple(cast(type<Ts>, st, static_cast<int>(Is))...);
  }

public:
  stmt_row(stmt_row const &) noexcept = default;
  stmt_row &operator=(stmt_row const &) noexcept = default;
  stmt_row(stmt_row &&) noexcept = default;
  stmt_row &operator=(stmt_row &&) noexcept = default;

  bool is_null(int column) const noexcept {
    return sqlite3_column_type(st, column) == SQLITE_NULL;
  }

  auto null_columns() const {
    return std::views::iota(0, sqlite3_column_count(st)) |
           std::views::filter([this](int col) { return is_null(col); });
  }

  template <typename T>
  T get(int column) const {
    return cast(type<T>, st, column);
  }

  template <typename... Ts>
  std::tuple<Ts...> get() const {
    return get_impl<Ts...>(std::make_index_sequence<sizeof...(Ts)>{});
  }

  // Convenience conversion operator, implicitly convert first column to T
  template <typename T>
  operator T() const {
    return get<T>(0);
  }
};

// statement iterator
struct stmt_sentinel {};

class stmt_iterator {
  stmt_raw *st;
  int rc;
  stmt_row current;

public:
  using iterator_category = std::input_iterator_tag;
  using value_type = stmt_row;
  using difference_type = std::ptrdiff_t;
  using pointer = stmt_row const *;
  using reference = stmt_row const &;

  stmt_iterator() noexcept = default;
  explicit stmt_iterator(stmt_raw *s) noexcept
      : st(s), rc(s ? sqlite3_step(s) : SQLITE_DONE), current(s) {}

  stmt_iterator(stmt_iterator const &) noexcept = default;
  stmt_iterator &operator=(stmt_iterator const &) noexcept = default;
  stmt_iterator(stmt_iterator &&) noexcept = default;
  stmt_iterator &operator=(stmt_iterator &&) noexcept = default;

  stmt_iterator &operator++() noexcept {
    if (st) {
      rc = sqlite3_step(st);
      if (rc != SQLITE_ROW) {
        // done or error
        st = nullptr;
      }
    }
    return *this;
  }

  stmt_row const &operator*() const noexcept { return current; }
  stmt_row const *operator->() const noexcept { return &current; }

  friend bool operator==(stmt_iterator const &lhs, stmt_sentinel) noexcept {
    // done or error
    return lhs.rc != SQLITE_ROW;
  }

  error state() const noexcept { return to_error(rc); }
};

class [[nodiscard("Statement finalised on discard.")]] stmt {
  struct deleter_type {
    void operator()(stmt_raw *s) const noexcept { sqlite3_finalize(s); }
  };
  std::unique_ptr<stmt_raw, deleter_type> st;

  template <int I, typename... Ts>
  error bind_impl(std::tuple<Ts...> const &args) const noexcept {
    if constexpr (I == sizeof...(Ts)) {
      return error::ok;
    } else {
      auto rc = sqlite::bind(handle(), I + 1, std::get<I>(args));
      if (rc != error::ok)
        return rc;
      return bind_impl<I + 1>(args);
    }
  }

public:
  stmt() noexcept = default;
  explicit stmt(stmt_raw *st) noexcept : st(st) {}

  stmt(stmt &&other) noexcept = default;
  stmt &operator=(stmt &&other) noexcept = default;

  explicit operator bool() const noexcept { return !!st; }

  stmt_raw *handle() const noexcept { return st.get(); }

  /**
   * @brief Binds values to the prepared statement.
   *
   * This function binds the provided values to the prepared statement in the order they
   * are provided.
   *
   * @tparam Ts Types of the values to bind.
   * @param args Values to bind to the prepared statement.
   * @return An `error` indicating the result of the binding operation.
   */
  template <typename... Ts>
  error bind(Ts &&...args) const noexcept {
    return bind_impl<0>(std::forward_as_tuple(std::forward<Ts>(args)...));
  }

  /**
   * @brief Binds a value to the prepared statement at a specific position.
   *
   * This function binds a single value to the prepared statement at the specified
   * position. The position is 1-based, meaning the first parameter is at position 1.
   *
   * @tparam POS The position in the prepared statement to bind the value to.
   * @tparam T The type of the value to bind.
   * @param value The value to bind to the prepared statement.
   * @return An `error` indicating the result of the binding operation.
   */
  template <int POS, typename T>
  error bind(T &&value) const noexcept {
    return bind_at(POS, std::forward<T>(value));
  }

  /**
   * @brief Binds a value to the prepared statement at a specific position.
   *
   * This function binds a single value to the prepared statement at the specified
   * position. The position is 1-based, meaning the first parameter is at position 1.
   *
   * @tparam T The type of the value to bind.
   * @param pos The position in the prepared statement to bind the value to (1-based
   * @param value The value to bind to the prepared statement.
   * @return An `error` indicating the result of the binding operation.
   */
  template <typename T>
  error bind_at(int pos, T &&value) const noexcept {
    return sqlite::bind(handle(), pos, std::forward<T>(value));
  }

  /**
   * @brief Binds a value to the prepared statement by name.
   *
   * This function binds a value to the prepared statement using the parameter name.
   *
   * @tparam T The type of the value to bind.
   * @param name The name of the parameter to bind the value to.
   * @param value The value to bind to the prepared statement.
   * @return An `error` indicating the result of the binding operation.
   */
  template <typename T>
  error bind_name(std::string const &name, T &&value) const noexcept {
    int pos = sqlite3_bind_parameter_index(handle(), name.c_str());
    if (pos == 0)
      return error::range; // parameter not found
    return bind_at(pos, std::forward<T>(value));
  }

  /**
   * @brief Clears all bindings from the prepared statement.
   *
   * @return An `error` indicating the result of the operation.
   */
  error clear_bindings() const noexcept {
    return to_error(sqlite3_clear_bindings(handle()));
  }

  /**
   * @brief Returns the number of parameters in the prepared statement.
   *
   * @return The number of parameters in the prepared statement.
   */
  int param_count() const noexcept { return sqlite3_bind_parameter_count(handle()); }

  auto param_names() const noexcept {
    return std::views::iota(1, param_count() + 1) |
           std::views::transform([s = handle()](int pos) {
             auto const *cstr = sqlite3_bind_parameter_name(s, pos);
             return cstr ? std::string{cstr} : std::string{};
           });
  }

  /**
   * @brief Returns an iterator to the beginning of the statement's result set.
   *
   * This function returns an iterator that can be used to iterate over the rows
   * returned by the prepared statement.
   *
   * @return An iterator to the beginning of the statement's result set.
   */
  stmt_iterator begin() const noexcept { return stmt_iterator{handle()}; }

  /**
   * @brief Returns a sentinel iterator indicating the end of the statement's result set.
   *
   * This function returns a sentinel iterator that indicates the end of the
   * statement's result set. It can be used in range-based for loops or other
   * iterator-based constructs.
   *
   * @return A sentinel iterator indicating the end of the statement's result set.
   */
  stmt_sentinel end() const noexcept { return {}; }

  /**
   * @brief Resets the prepared statement.
   *
   * This function resets the prepared statement, allowing it to be reused for
   * subsequent executions. If `clear_bindings` is true, it also clears any
   * bindings that were previously set.
   *
   * @param clear_bindings If true, clears all bindings from the prepared statement.
   * @return An `error` indicating the result of the reset operation.
   */
  error reset(bool clear_bindings = false) const noexcept {
    auto rc = to_error(sqlite3_reset(handle()));
    if (is_ok(rc) && clear_bindings) {
      rc = this->clear_bindings();
    }
    return rc;
  }

  /**
   * @brief Executes the prepared statement.
   *
   * This function executes the prepared statement and returns an `error` indicating
   * the result of the execution. If `reset` is true, it resets the statement after
   * execution, optionally clearing bindings if `reset_clear_bindings` is true.
   *
   * @param reset If true, resets the statement after execution.
   * @param reset_clear_bindings If true, clears all bindings after resetting.
   * @return An `error` indicating the result of the execution.
   */
  [[nodiscard]] error exec(bool reset = false,
                           bool reset_clear_bindings = false) const noexcept {
    auto it = begin();
    auto rc = error::done;
    while (it != end()) {
      ++it;
      rc = it.state();
    }
    if (reset) {
      rc = this->reset(reset_clear_bindings);
    }
    return rc;
  }

  /**
   * @brief Binds values to the prepared statement and executes it.
   *
   * This function binds the provided values to the prepared statement and then
   * executes it. If binding fails, it clears the bindings before returning the error.
   * It also clears and reset the statement after execution.
   *
   * @tparam Ts Types of the values to bind.
   * @param args Values to bind to the prepared statement.
   * @return An `error` indicating the result of the binding and execution operation.
   */
  template <typename... Ts>
  [[nodiscard]] error bind_exec(Ts &&...args) const noexcept {
    auto rc = bind(std::forward<Ts>(args)...);
    if (is_ok(rc)) {
      rc = exec(true, true);
    } else {
      // If binding failed, clear bindings before returning the error
      clear_bindings();
    }
    return rc;
  }

  /**
   * @brief Returns the number of columns in the result set of the prepared statement.
   *
   * @return The number of columns in the result set.
   * @note This function is only valid after executing the statement.
   */
  int column_count() const noexcept { return sqlite3_column_count(handle()); }

  /**
   * @brief Returns the type of the column at the specified index.
   *
   * This function returns the type of the column at the specified index in the result
   * set. The index is 0-based, meaning the first column is at index 0.
   *
   * @param col The index of the column to get the type for (0-based).
   * @return The type of the column as a `value_type` enum.
   */
  auto column_type(int col) const noexcept {
    return static_cast<value_type>(sqlite3_column_type(handle(), col));
  }

  /**
   * @brief Returns a range of column types for all columns in the result set.
   *
   * This function returns a range of column types for all columns in the result set,
   * allowing iteration over the types of each column.
   *
   * @return A range of `value_type` enums representing the types of each column.
   */
  auto column_types() const {
    return std::views::iota(0, column_count()) |
           std::views::transform([this](int col) { return column_type(col); });
  }

  /**
   * @brief Returns the name of the column at the specified index.
   *
   * This function returns the name of the column at the specified index in the result
   * set. The index is 0-based, meaning the first column is at index 0.
   *
   * @param col The index of the column to get the name for (0-based).
   * @return The name of the column as a `std::string`.
   */
  auto column_name(int col) const {
    auto cstr = sqlite3_column_name(handle(), col);
    // If nullptr here we have a bigger problem: malloc failure in sqlite3
    return cstr ? std::string{cstr} : std::string{};
  }

  /**
   * @brief Returns a range of column names for all columns in the result set.
   *
   * This function returns a range of column names for all columns in the result set,
   * allowing iteration over the names of each column.
   *
   * @return A range of `std::string` representing the names of each column.
   */
  auto column_names() const {
    return std::views::iota(0, column_count()) |
           std::views::transform([this](int col) { return column_name(col); });
  }
};

/**
 * @brief Prepares a SQLite statement from the provided SQL string.
 *
 * This function prepares a SQLite statement using the provided SQL string and returns
 * a `stmt` object that manages the prepared statement. If preparation fails, it returns
 * an empty `stmt`.
 *
 * @param db Pointer to the raw SQLite database connection.
 * @param sql The SQL statement to prepare as a string.
 * @return A `stmt` object managing the prepared statement. If preparation fails,
 * returns an empty `stmt`.
 */
inline stmt prepare_stmt(conn_raw *db, std::string const &sql) {
  stmt_raw *raw_stmt = nullptr;
  auto rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt, nullptr);
  if (rc != SQLITE_OK) {
    return stmt{nullptr}; // Prepare failed
  }
  return stmt{raw_stmt};
}

} // namespace ju::sqlite
