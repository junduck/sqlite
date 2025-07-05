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

class stmt {
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

  template <typename... Ts>
  error bind(Ts &&...args) const noexcept {
    return bind_impl<0>(std::forward_as_tuple(std::forward<Ts>(args)...));
  }

  template <int POS, typename T>
  error bind(T &&value) const noexcept {
    return bind_at(POS, std::forward<T>(value));
  }

  template <typename T>
  error bind_at(int pos, T &&value) const noexcept {
    return sqlite::bind(handle(), pos, std::forward<T>(value));
  }

  template <typename T>
  error bind_name(std::string const &name, T &&value) const noexcept {
    int pos = sqlite3_bind_parameter_index(handle(), name.c_str());
    if (pos == 0)
      return error::range; // parameter not found
    return bind_at(pos, std::forward<T>(value));
  }

  error clear_bindings() const noexcept {
    return to_error(sqlite3_clear_bindings(handle()));
  }

  int param_count() const noexcept { return sqlite3_bind_parameter_count(handle()); }

  auto param_names() const noexcept {
    return std::views::iota(1, param_count() + 1) |
           std::views::transform([s = handle()](int pos) {
             auto const *cstr = sqlite3_bind_parameter_name(s, pos);
             return cstr ? std::string{cstr} : std::string{};
           });
  }

  stmt_iterator begin() const noexcept { return stmt_iterator{handle()}; }

  stmt_sentinel end() const noexcept { return {}; }

  error reset(bool clear_bindings = false) const noexcept {
    auto rc = to_error(sqlite3_reset(handle()));
    if (is_ok(rc) && clear_bindings) {
      rc = this->clear_bindings();
    }
    return rc;
  }

  error exec(bool reset = false, bool reset_clear_bindings = false) const noexcept {
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

  int column_count() const noexcept { return sqlite3_column_count(handle()); }

  auto column_type(int col) const noexcept {
    return static_cast<value_type>(sqlite3_column_type(handle(), col));
  }

  auto column_types() const {
    return std::views::iota(0, column_count()) |
           std::views::transform([this](int col) { return column_type(col); });
  }

  auto column_name(int col) const {
    auto cstr = sqlite3_column_name(handle(), col);
    // If nullptr here we have a bigger problem: malloc failure in sqlite3
    return cstr ? std::string{cstr} : std::string{};
  }

  auto column_names() const {
    return std::views::iota(0, column_count()) |
           std::views::transform([this](int col) { return column_name(col); });
  }
};

inline stmt prepare_stmt(conn_raw *db, std::string const &sql) {
  stmt_raw *raw_stmt = nullptr;
  auto rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt, nullptr);
  if (rc != SQLITE_OK) {
    return stmt{nullptr}; // Prepare failed
  }
  return stmt{raw_stmt};
}

} // namespace ju::sqlite
