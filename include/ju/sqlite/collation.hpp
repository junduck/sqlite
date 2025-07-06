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

/**
 * @brief Registers a custom collation function with the SQLite database connection.
 *
 * This overload is used for stateful collation objects, where a pointer to the collation
 * object is provided and stored as user data in SQLite. The collation object must satisfy
 * the `collation` concept, i.e., it must be callable with two `std::string_view`
 * arguments and return an `int` (compatible with SQLite's collation requirements).
 *
 * @tparam T Type of the collation object, satisfying the `collation` concept.
 * @param db Pointer to the raw SQLite database connection.
 * @param name Name of the collation as a string.
 * @param coll Pointer to the collation object to be used for string comparison.
 * @return An `error` indicating the result of the registration operation.
 *
 * @note The lifetime of the collation object pointed to by `coll` must outlive the
 *       registration with SQLite, as SQLite will use this pointer for collation
 *       operations.
 */
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

/**
 * @brief Registers a custom collation function with the SQLite database connection.
 *
 * This overload is used for stateful collation objects, where a managed object is created
 * to hold the collation state. The collation object must satisfy the `collation` concept,
 * i.e., it must be callable with two `std::string_view` arguments and return an `int`
 * (compatible with SQLite's collation requirements).
 *
 * @tparam T Type of the collation object, satisfying the `collation` concept.
 * @param db Pointer to the raw SQLite database connection.
 * @param name Name of the collation as a string.
 * @param coll Collation object to be used for string comparison.
 * @return An `error` indicating the result of the registration operation.
 */
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

/**
 * @brief Creates a custom collation function in the SQLite database.
 *
 * This function registers a collation function of type `T` with the SQLite database
 * connection. It can handle both stateful and stateless collations, depending on the
 * template parameter `T`.
 *
 * @tparam T Type of the collation object, satisfying the `collation` concept.
 * @param db Pointer to the raw SQLite database connection.
 * @param name Name of the collation as a string.
 * @param args Arguments forwarded to the constructor of the collation object.
 * @return An `error` indicating the result of the registration operation.
 */
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

/**
 * @brief Creates a stateless collation function in the SQLite database.
 *
 * This function registers a stateless collation function of type `T` with the SQLite
 * database connection. The collation object must satisfy the `collation` concept, i.e.,
 * it must be callable with two `std::string_view` arguments and return an `int`
 * (compatible with SQLite's collation requirements).
 *
 * @tparam T Type of the collation object, satisfying the `collation` concept.
 * @param db Pointer to the raw SQLite database connection.
 * @param name Name of the collation as a string.
 * @return An `error` indicating the result of the registration operation.
 */
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
