#pragma once

#include "sqlite3.h"

#include <tuple>
#include <type_traits>

namespace ju::sqlite {

// SQLite3 native types
using conn_raw = ::sqlite3;
using context_raw = ::sqlite3_context;
using stmt_raw = ::sqlite3_stmt;
using value_raw = ::sqlite3_value;
using backup_raw = ::sqlite3_backup;
using destructor_type_raw = ::sqlite3_destructor_type;

template <typename T>
struct type_t {};

template <typename T>
constexpr inline type_t<T> type{};

template <typename T>
concept simple =
    std::is_trivially_constructible_v<T> && std::is_trivially_destructible_v<T>;

template <typename T>
concept stateless = std::is_empty_v<T> && simple<T>;

template <typename T>
concept functor = requires { &T::operator(); };

template <typename T>
concept aggregator = requires {
  &T::step;
  &T::value;
};

template <typename T>
concept invertible = requires { &T::inverse; };

namespace detail {
template <typename>
struct tuple_split {};

template <typename H, typename... T>
struct tuple_split<std::tuple<H, T...>> {
  using head_type = H;
  using tail_type = std::tuple<T...>;
};

template <>
struct tuple_split<std::tuple<>> {
  using head_type = void;
  using tail_type = std::tuple<>;
};

template <typename, typename>
struct tuple_concat {};

template <typename... Ts, typename... Us>
struct tuple_concat<std::tuple<Ts...>, std::tuple<Us...>> {
  using type = std::tuple<Ts..., Us...>;
};

template <typename Tuple>
using tuple_head_t = detail::tuple_split<Tuple>::head_type;

template <typename Tuple>
using tuple_tail_t = detail::tuple_split<Tuple>::tail_type;

template <typename Tup1, typename Tup2>
using tuple_concat_t = detail::tuple_concat<Tup1, Tup2>::type;
} // namespace detail
} // namespace ju::sqlite
