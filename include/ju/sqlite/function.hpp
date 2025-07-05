#pragma once

#include <tuple>

#include "common.hpp"
#include "context.hpp"
#include "defines.hpp"
#include "error.hpp"
#include "tag.hpp"

namespace ju::sqlite {
namespace detail {
namespace impl {
template <bool Ctx, typename Ret, typename... Args>
struct callable_traits_impl_base {
  using return_type = Ret;

  using call_args =
      std::conditional_t<Ctx, std::tuple<context_raw *, Args...>, std::tuple<Args...>>;
  using sql_args = std::tuple<Args...>;

  constexpr static bool needs_context = Ctx;
  // Arity of the SQL function, thus sans context.
  constexpr static auto arity = sizeof...(Args);

  template <size_t... Is>
  static call_args build_call_args_impl(context_raw *ctx,
                                        value_raw **argv,
                                        std::integer_sequence<size_t, Is...>) {
    if constexpr (needs_context) {
      return std::make_tuple(ctx, cast(type<Args>, argv[Is])...);
    } else {
      return std::make_tuple(cast(type<Args>, argv[Is])...);
    }
  }

  static call_args build_call_args(context_raw *ctx, value_raw **argv) {
    return build_call_args_impl(ctx, argv, std::make_integer_sequence<size_t, arity>{});
  }
};

template <typename Sig>
struct callable_traits_impl;

template <typename Ret, typename... Args>
struct callable_traits_impl<Ret (*)(context_raw *, Args...)>
    : callable_traits_impl_base<true, Ret, Args...> {
  using signature_type = Ret(context_raw *, Args...);
};

template <typename Ret, typename... Args>
struct callable_traits_impl<Ret (*)(Args...)>
    : callable_traits_impl_base<false, Ret, Args...> {
  using signature_type = Ret(Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct callable_traits_impl<Ret (Cls::*)(context_raw *, Args...)>
    : callable_traits_impl_base<true, Ret, Args...> {
  using signature_type = Ret (Cls::*)(context_raw *, Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct callable_traits_impl<Ret (Cls::*)(Args...)>
    : callable_traits_impl_base<false, Ret, Args...> {
  using signature_type = Ret (Cls::*)(Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct callable_traits_impl<Ret (Cls::*)(context_raw *, Args...) const>
    : callable_traits_impl_base<true, Ret, Args...> {
  using signature_type = Ret (Cls::*)(context_raw *, Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct callable_traits_impl<Ret (Cls::*)(Args...) const>
    : callable_traits_impl_base<false, Ret, Args...> {
  using signature_type = Ret (Cls::*)(Args...);
};
} // namespace impl

template <typename T>
struct callable_traits {
  using type = impl::callable_traits_impl<std::decay_t<T>>;
};

template <functor T>
struct callable_traits<T> {
  using type = impl::callable_traits_impl<decltype(&T::operator())>;
};

template <typename T>
using callable_traits_t = typename callable_traits<T>::type;

template <typename F>
constexpr inline auto invoke_userdata_func =
    +[](context_raw *ctx, int argc, value_raw **argv) {
      using trait = callable_traits_t<F>;
      static_cast<void>(argc);
      F *callable = userdata<F>(ctx);
      JUSQLITE_TRY_CTX({
        // Check if return type is void
        if constexpr (std::is_void_v<typename trait::return_type>) {
          static_assert(trait::needs_context,
                        "Void return callable must set result in context.");
          std::apply(*callable, trait::build_call_args(ctx, argv));
          return;
        } else {
          bind(ctx,
               std::apply(*callable, callable_traits_t<F>::build_call_args(ctx, argv)));
        }
      });
    };

template <typename F>
constexpr inline auto invoke_stateless_func =
    +[](context_raw *ctx, int argc, value_raw **argv) {
      using trait = callable_traits_t<F>;
      static_cast<void>(argc);
      JUSQLITE_TRY_CTX({
        // Check if return type is void
        if constexpr (std::is_void_v<typename trait::return_type>) {
          static_assert(trait::needs_context,
                        "Void return callable must set result in context.");
          std::apply(F{}, trait::build_call_args(ctx, argv));
          return;
        } else {
          bind(ctx, std::apply(F{}, callable_traits_t<F>::build_call_args(ctx, argv)));
        }
      });
    };
} // namespace detail

/**
 * @brief Register a user-defined function in the SQLite database.
 *
 * This function allows you to register a user-defined function, referenced by pointer
 * `callable`, in the SQLite database. Since lifetime of the callable is not managed by
 * SQLite, you must ensure that the callable remains valid for the duration of its use.
 *
 * @tparam T Callable type.
 * @param db Current database connection.
 * @param name Name of the function to register.
 * @param flags Flags for the function, such as SQLITE_UTF8 or
 * SQLITE_DETERMINISTIC.
 * @param callable Pointer to the callable to register.
 * @return error
 */
template <typename T>
error register_function(conn_raw *db, std::string const &name, int flags, T *callable) {
  using trait = detail::callable_traits_t<T>;
  static_assert(trait::arity <= 127, "SQLite function arity must be between 0 and 127");
  void *storage = void_cast(callable);
  auto rc = sqlite3_create_function_v2(db,
                                       name.c_str(),
                                       trait::arity,
                                       flags,
                                       storage,
                                       detail::invoke_userdata_func<T>,
                                       nullptr,
                                       nullptr,
                                       nullptr);
  return to_error(rc);
}

/**
 * @brief Register a user-defined function in the SQLite database.
 *
 * This function allows you to register a user-defined function, moved constructed from
 * `callable`, in the SQLite database. The callable is managed by SQLite, so it will be
 * automatically cleaned up when the database connection is closed or the function is not
 * needed anymore.
 *
 * @tparam T Callable type.
 */
template <typename T>
error register_function(conn_raw *db, std::string const &name, int flags, T &&callable)
  requires(!std::is_pointer_v<std::decay_t<T>>)
{
  using trait = detail::callable_traits_t<T>;
  static_assert(trait::arity <= 127, "SQLite function arity must be between 0 and 127");

  auto storage = make_managed<T>(static_cast<T &&>(callable));
  if (!storage)
    return error::nomem;

  auto rc = sqlite3_create_function_v2(db,
                                       name.c_str(),
                                       trait::arity,
                                       flags,
                                       storage,
                                       detail::invoke_userdata_func<T>,
                                       nullptr,
                                       nullptr,
                                       deleter(storage));
  return to_error(rc);
}

/**
 * @brief Create a user-defined function in the SQLite database.
 *
 * The function allows you to create a user-defined function, constructed in place
 * from the provided callable type `F`, in the SQLite database. The callable is
 * managed by SQLite, so it will be automatically cleaned up when the database
 * connection is closed or the function is not needed anymore.
 *
 * @tparam F Callable type.
 * @tparam Args Constructor argument types for the function object.
 * @param db Current database connection.
 * @param name Name of the function to create.
 * @param flag Flags for the function, such as SQLITE_UTF8 or
 * SQLITE_DETERMINISTIC.
 * @param args Constructor arguments for the function object.
 * @return error
 */
template <typename F, typename... Args>
error create_function(conn_raw *db, std::string const &name, int flag, Args &&...args) {
  using trait = detail::callable_traits_t<F>;
  static_assert(trait::arity <= 127, "SQLite function arity must be between 0 and 127");

  auto storage = make_managed<F>(static_cast<Args &&>(args)...);
  if (!storage)
    return error::nomem;
  auto rc = sqlite3_create_function_v2(db,
                                       name.c_str(),
                                       trait::arity,
                                       flag,
                                       storage,
                                       detail::invoke_userdata_func<F>,
                                       nullptr,
                                       nullptr,
                                       deleter(storage));
  return to_error(rc);
}

template <stateless F>
error create_function(conn_raw *db, std::string const &name, int flag) {
  using trait = detail::callable_traits_t<F>;
  static_assert(trait::arity <= 127, "SQLite function arity must be between 0 and 127");

  auto rc = sqlite3_create_function_v2(db,
                                       name.c_str(),
                                       trait::arity,
                                       flag,
                                       nullptr,
                                       detail::invoke_stateless_func<F>,
                                       nullptr,
                                       nullptr,
                                       nullptr);
  return to_error(rc);
}
} // namespace ju::sqlite
