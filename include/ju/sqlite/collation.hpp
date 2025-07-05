#pragma once

#include "pointer.hpp"

namespace ju::sqlite {
template <typename T>
concept collation = requires(T t) {
  { t(std::string_view{}, std::string_view{}) } noexcept -> std::convertible_to<int>;
};

namespace detail {
template <collation T>
constexpr inline auto invoke_stateful_collation =
    +[](void *storage, int lhs_len, const void *lhs, int rhs_len, const void *rhs)
    -> int {
  std::string_view lhs_sv{static_cast<const char *>(lhs), static_cast<size_t>(lhs_len)};
  std::string_view rhs_sv{static_cast<const char *>(rhs), static_cast<size_t>(rhs_len)};
  T *coll = pointer_cast<T>(storage);
  return std::invoke(*coll, lhs_sv, rhs_sv);
};

template <collation T>
constexpr inline auto invoke_stateless_collation =
    +[](void *, int lhs_len, const void *lhs, int rhs_len, const void *rhs) -> int {
  std::string_view lhs_sv{static_cast<const char *>(lhs), static_cast<size_t>(lhs_len)};
  std::string_view rhs_sv{static_cast<const char *>(rhs), static_cast<size_t>(rhs_len)};
  return T{}(lhs_sv, rhs_sv);
};
} // namespace detail

template <collation T>
error register_collation(conn_raw *db, std::string const &name, T *coll) {
  void *storage = void_cast(coll);
  auto rc = sqlite3_create_collation_v2(db,
                                        name.c_str(),
                                        SQLITE_UTF8,
                                        storage,
                                        detail::invoke_stateful_collation<T>,
                                        nullptr);
  return to_error(rc);
}

template <collation T>
error register_collation(conn_raw *db, std::string const &name, T &&coll)
  requires(!std::is_pointer_v<std::decay_t<T>>)
{
  auto storage = make_managed<T>(static_cast<T &&>(coll));
  if (!storage)
    return error::nomem;
  auto rc = sqlite3_create_collation_v2(db,
                                        name.c_str(),
                                        SQLITE_UTF8,
                                        storage,
                                        detail::invoke_stateful_collation<T>,
                                        deleter(storage));
  return to_error(rc);
}

template <collation T, typename... Args>
error create_collation(conn_raw *db, std::string const &name, Args &&...args) {
  auto storage = make_managed<T>(static_cast<Args &&>(args)...);
  if (!storage)
    return error::nomem;
  auto rc = sqlite3_create_collation_v2(db,
                                        name.c_str(),
                                        SQLITE_UTF8,
                                        storage,
                                        detail::invoke_stateful_collation<T>,
                                        deleter(storage));
  return to_error(rc);
}

template <collation T>
error create_collation(conn_raw *db, std::string const &name)
  requires(stateless<T>)
{
  auto rc = sqlite3_create_collation_v2(db,
                                        name.c_str(),
                                        SQLITE_UTF8,
                                        nullptr,
                                        detail::invoke_stateless_collation<T>,
                                        nullptr);
  return to_error(rc);
}
} // namespace ju::sqlite
