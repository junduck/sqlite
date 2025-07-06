#pragma once

#include "common.hpp"
#include "context.hpp"
#include "defines.hpp"

namespace ju::sqlite {
namespace detail {

// SQLite3 expected aggregate signatures
using step_fn = void (*)(context_raw *, int, value_raw **);
using inverse_fn = step_fn;
using value_fn = void (*)(context_raw *);
using final_fn = value_fn;

namespace impl {
template <bool Ctx, typename Ret, typename Cls, typename... Args>
struct method_traits_impl_base {
  using return_type = Ret;

  using call_args = std::conditional_t<Ctx,
                                       std::tuple<Cls *, context_raw *, Args...>,
                                       std::tuple<Cls *, Args...>>;
  using sql_args = std::tuple<Args...>;

  constexpr static bool needs_context = Ctx;
  // Arity of the SQL function, thus sans context.
  constexpr static auto arity = sizeof...(Args);

  template <size_t... Is>
  static call_args build_call_args_impl(Cls *obj,
                                        context_raw *ctx,
                                        value_raw **argv,
                                        std::integer_sequence<size_t, Is...>) {
    if constexpr (needs_context) {
      return std::make_tuple(obj, ctx, cast(type<Args>, argv[Is])...);
    } else {
      return std::make_tuple(obj, cast(type<Args>, argv[Is])...);
    }
  }

  static call_args build_call_args(Cls *obj, context_raw *ctx, value_raw **argv) {
    return build_call_args_impl(
        obj, ctx, argv, std::make_integer_sequence<size_t, arity>{});
  }
};

template <typename Sig>
struct method_traits_impl;

template <typename Ret, typename Cls, typename... Args>
struct method_traits_impl<Ret (Cls::*)(context_raw *, Args...)>
    : method_traits_impl_base<true, Ret, std::remove_const_t<Cls>, Args...> {
  using signature_type = Ret (Cls::*)(context_raw *, Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct method_traits_impl<Ret (Cls::*)(context_raw *, Args...) const>
    : method_traits_impl_base<true, Ret, std::add_const_t<Cls>, Args...> {
  using signature_type = Ret (Cls::*)(context_raw *, Args...) const;
};

template <typename Ret, typename Cls, typename... Args>
struct method_traits_impl<Ret (Cls::*)(Args...)>
    : method_traits_impl_base<false, Ret, std::remove_const_t<Cls>, Args...> {
  using signature_type = Ret (Cls::*)(Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct method_traits_impl<Ret (Cls::*)(Args...) const>
    : method_traits_impl_base<false, Ret, std::add_const_t<Cls>, Args...> {
  using signature_type = Ret (Cls::*)(Args...) const;
};
} // namespace impl

template <typename T>
struct method_traits {
  using type = impl::method_traits_impl<std::decay_t<T>>;
};

template <typename T>
using method_traits_t = typename method_traits<T>::type;

template <typename T>
using step_traits_t = method_traits_t<decltype(&T::step)>;

template <typename T>
using inverse_traits_t = method_traits_t<decltype(&T::inverse)>;

template <typename T>
constexpr inline auto invoke_agg_step =
    +[](context_raw *ctx, int argc, value_raw **argv) {
      static_cast<void>(argc);
      auto *agg = aggdata<T>(ctx);
      JUSQLITE_VALIDATE_CTX(agg, ctx);

      JUSQLITE_TRY_CTX(
          { std::apply(&T::step, step_traits_t<T>::build_call_args(agg, ctx, argv)); });
    };

template <typename T>
constexpr inline auto invoke_agg_value = +[](context_raw *ctx) {
  auto *agg = aggdata<T>(ctx);
  JUSQLITE_VALIDATE_CTX(agg, ctx);

  JUSQLITE_TRY_CTX({ bind(ctx, agg->value()); });
};

template <typename T>
constexpr inline auto invoke_agg_inverse =
    +[](context_raw *ctx, int argc, value_raw **argv) {
      static_cast<void>(argc);
      auto *agg = aggdata<T>(ctx);
      JUSQLITE_VALIDATE_CTX(agg, ctx);

      JUSQLITE_TRY_CTX({
        std::apply(&T::inverse, inverse_traits_t<T>::build_call_args(agg, ctx, argv));
      });
    };

template <typename T>
constexpr inline auto invoke_agg_final = +[](context_raw *ctx) {
  auto *agg = aggdata<T>(ctx);
  JUSQLITE_VALIDATE_CTX(agg, ctx);

  JUSQLITE_TRY_CTX({ bind(ctx, agg->value()); });
  std::destroy_at(agg);
};
} // namespace detail

/**
 * @brief Creates and registers a custom aggregate (or window) function with SQLite.
 *
 * This function sets up an aggregate or window function of type `T` in the given SQLite
 * database connection. It handles the registration of the step, inverse, value, and final
 * callbacks, as well as the management of any state required by the aggregate function.
 *
 * @tparam T      The aggregate function class type. Must provide at least a `step` and
 * `value` methods, and optionally `inverse` method for window functions.
 * @tparam Args   Types of constructor arguments for the aggregate state object.
 * @param db      SQLite database connection pointer.
 * @param name    Name of the aggregate function as it will be used in SQL.
 * @param flag    Flags for SQLite function creation (e.g., SQLITE_UTF8).
 * @param args    Arguments forwarded to the constructor of the aggregate state object.
 * @return        An `error` enum indicating success or the type of failure.
 *
 * @note
 * - If `T` is not a "simple" aggregate, a control object is allocated and managed.
 * - If `T` is invertible (supports window functions), the inverse and value callbacks are
 * registered.
 * - The function uses `sqlite3_create_window_function` to register the aggregate or
 * window function.
 * - The function ensures proper memory management and error handling for the aggregate
 * state.
 */
template <typename T, typename... Args>
error create_aggregate(conn_raw *db, std::string const &name, int flag, Args &&...args) {

  void *ctrl = nullptr;
  destructor_type_raw ctrl_d = nullptr;

  detail::step_fn xstep = nullptr;
  detail::inverse_fn xinverse = nullptr;
  detail::value_fn xvalue = nullptr;
  detail::final_fn xfinal = nullptr;

  if constexpr (!simple<T>) {
    using control_t = detail::aggregate_control<T, Args...>;
    auto p = make_managed<control_t>(static_cast<Args &&>(args)...);
    if (!p)
      return error::nomem;
    p->initialise = detail::construct_aggregate<T, Args...>;
    ctrl = p;
    ctrl_d = deleter(p);
  }

  xstep = detail::invoke_agg_step<T>;
  if constexpr (invertible<T>) {
    xinverse = detail::invoke_agg_inverse<T>;
    xvalue = detail::invoke_agg_value<T>;
  }
  xfinal = detail::invoke_agg_final<T>;

  auto rc = sqlite3_create_window_function(db,
                                           name.c_str(),
                                           detail::step_traits_t<T>::arity,
                                           flag,
                                           ctrl,
                                           xstep,
                                           xfinal,
                                           xvalue,
                                           xinverse,
                                           ctrl_d);
  return to_error(rc);
};
} // namespace ju::sqlite
