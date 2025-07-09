#pragma once

#include <system_error>

#include <ju/tag_invoke.hpp>

#include "blob.hpp"
#include "common.hpp"
#include "error.hpp"

namespace ju::sqlite {

namespace tag {

struct deleter_tag {
  // NOTE: This means default is to copy data into blob, which should be owning.
  template <typename T>
  friend auto tag_invoke(deleter_tag, T &&) noexcept {
    return SQLITE_TRANSIENT;
  }
  friend auto tag_invoke(deleter_tag, text_view) noexcept { return SQLITE_STATIC; }
  friend auto tag_invoke(deleter_tag, blob_view) noexcept { return SQLITE_STATIC; }

  DISPATCH_FOR(deleter_tag)
};
} // namespace tag

inline constexpr tag::deleter_tag deleter{};

namespace tag {
struct cast_tag {
  friend auto tag_invoke(cast_tag, type_t<double>, value_raw *val) noexcept {
    return sqlite3_value_double(val);
  }
  friend auto tag_invoke(cast_tag, type_t<int>, value_raw *val) noexcept {
    return sqlite3_value_int(val);
  }
  friend auto tag_invoke(cast_tag, type_t<int64_t>, value_raw *val) noexcept {
    return sqlite3_value_int64(val);
  }
  // TODO: handling value_raw* may be dangerous due to the "protected value" design
  // Owning wrappers to make value and protected_value different types?
  friend auto tag_invoke(cast_tag, type_t<value_raw *>, value_raw *val) noexcept {
    return val;
  }
  template <text_like T>
  friend auto tag_invoke(cast_tag, type_t<T>, value_raw *val) noexcept(
      std::is_nothrow_constructible_v<std::decay_t<T>, char const *, size_t>) {
    auto p = sqlite3_value_text(val);
    auto n = sqlite3_value_bytes(val);
    return std::decay_t<T>{reinterpret_cast<char const *>(p), static_cast<size_t>(n)};
  }
  template <blob_like T>
  friend auto tag_invoke(cast_tag, type_t<T>, value_raw *val) noexcept(
      std::is_nothrow_constructible_v<std::decay_t<T>, unsigned char const *, size_t>) {
    auto p = sqlite3_value_blob(val);
    auto n = sqlite3_value_bytes(val);
    return std::decay_t<T>{static_cast<unsigned char const *>(p), static_cast<size_t>(n)};
  }

  friend auto tag_invoke(cast_tag, type_t<double>, stmt_raw *st, int icol) noexcept {
    return sqlite3_column_double(st, icol);
  }
  friend auto tag_invoke(cast_tag, type_t<int>, stmt_raw *st, int icol) noexcept {
    return sqlite3_column_int(st, icol);
  }
  friend auto tag_invoke(cast_tag, type_t<int64_t>, stmt_raw *st, int icol) noexcept {
    return sqlite3_column_int64(st, icol);
  }
  friend auto tag_invoke(cast_tag, type_t<stmt_raw *>, stmt_raw *st, int icol) noexcept {
    std::ignore = icol;
    return st;
  }
  template <text_like T>
  friend auto tag_invoke(cast_tag, type_t<T>, stmt_raw *st, int icol) noexcept(
      std::is_nothrow_constructible_v<std::decay_t<T>, char const *, size_t>) {
    auto p = sqlite3_column_text(st, icol);
    auto n = sqlite3_column_bytes(st, icol);
    return std::decay_t<T>{reinterpret_cast<char const *>(p), static_cast<size_t>(n)};
  }
  template <blob_like T>
  friend auto tag_invoke(cast_tag, type_t<T>, stmt_raw *st, int icol) noexcept(
      std::is_nothrow_constructible_v<std::decay_t<T>, unsigned char const *, size_t>) {
    auto p = sqlite3_column_blob(st, icol);
    auto n = sqlite3_column_bytes(st, icol);
    return std::decay_t<T>{static_cast<unsigned char const *>(p), static_cast<size_t>(n)};
  }

  DISPATCH_FOR(cast_tag)
};

struct bind_tag {
  friend error tag_invoke(bind_tag, stmt_raw *st, int icol, double v) noexcept {
    auto rc = sqlite3_bind_double(st, icol, v);
    return to_error(rc);
  }
  friend error tag_invoke(bind_tag, stmt_raw *st, int icol, int v) noexcept {
    auto rc = sqlite3_bind_int(st, icol, v);
    return to_error(rc);
  }
  friend error tag_invoke(bind_tag, stmt_raw *st, int icol, int64_t v) noexcept {
    auto rc = sqlite3_bind_int64(st, icol, v);
    return to_error(rc);
  }
  friend error tag_invoke(bind_tag, stmt_raw *st, int icol, nullptr_t) noexcept {
    auto rc = sqlite3_bind_null(st, icol);
    return to_error(rc);
  }
  friend error
  tag_invoke(bind_tag, stmt_raw *st, int icol, blob_like auto &&blob) noexcept {
    auto rc = sqlite3_bind_blob64(
        st, icol, std::ranges::data(blob), std::ranges::size(blob), deleter(blob));
    return to_error(rc);
  }
  friend error
  tag_invoke(bind_tag, stmt_raw *st, int icol, text_like auto &&text) noexcept {
    auto rc = sqlite3_bind_text64(st,
                                  icol,
                                  std::ranges::data(text),
                                  std::ranges::size(text),
                                  deleter(text),
                                  SQLITE_UTF8);
    return to_error(rc);
  }
  friend error tag_invoke(bind_tag, stmt_raw *st, int icol, char const *c_str) noexcept {
    auto rc = sqlite3_bind_text64(
        st, icol, c_str, std::strlen(c_str), SQLITE_STATIC, SQLITE_UTF8);
    return to_error(rc);
  }

  friend void tag_invoke(bind_tag, context_raw *ctx, double v) noexcept {
    sqlite3_result_double(ctx, v);
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, int v) noexcept {
    sqlite3_result_int(ctx, v);
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, int64_t v) noexcept {
    sqlite3_result_int64(ctx, v);
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, nullptr_t) noexcept {
    sqlite3_result_null(ctx);
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, blob_like auto &&blob) noexcept {
    sqlite3_result_blob64(
        ctx, std::ranges::data(blob), std::ranges::size(blob), deleter(blob));
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, text_like auto &&text) noexcept {
    sqlite3_result_text64(ctx,
                          std::ranges::data(text),
                          std::ranges::size(text),
                          deleter(text),
                          SQLITE_UTF8);
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, char const *c_str) noexcept {
    sqlite3_result_text64(ctx, c_str, std::strlen(c_str), SQLITE_STATIC, SQLITE_UTF8);
  }

  friend void tag_invoke(bind_tag, context_raw *ctx, error e) noexcept {
    sqlite3_result_error_code(ctx, static_cast<int>(e));
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, std::exception const &e) noexcept {
    sqlite3_result_error(ctx, e.what(), -1);
  }
  friend void tag_invoke(bind_tag, context_raw *ctx, std::error_code const &e) noexcept {
    sqlite3_result_error(ctx, e.message().c_str(), -1);
  }

  DISPATCH_FOR(bind_tag)
};
} // namespace tag

inline constexpr tag::cast_tag cast{};

inline constexpr tag::bind_tag bind{};

} // namespace ju::sqlite
