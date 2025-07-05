#pragma once

#include <memory>
#include <tuple>
#include <type_traits>

#include "common.hpp"
#include "pointer.hpp"

namespace ju::sqlite {

template <typename T>
T *userdata(context_raw *ctx) {
  return pointer_cast<T>(sqlite3_user_data(ctx));
}

template <typename T = void>
T *auxdata(context_raw *ctx, int idx) {
  return pointer_cast<T>(sqlite3_get_auxdata(ctx, idx));
}

namespace detail {

template <typename T>
using aggregate_initialiser_fn = void (*)(T *, void *);

template <typename T>
struct aggregate_control_base {
  aggregate_initialiser_fn<T> initialise = nullptr;
};

template <typename T, typename... Args>
struct aggregate_control : aggregate_control_base<T> {
  using base = aggregate_control_base<T>;
  std::tuple<std::unwrap_ref_decay_t<Args>...> constructor_args;

  aggregate_control(Args &&...args)
      : base{}, constructor_args(std::forward<Args>(args)...) {}
};

/**
 * @brief Type-erased aggregate initialiser for deferred construction
 *
 * SQLite separates allocation and initialisation of aggregate context.
 * This function enables constructor argument forwarding by storing
 * constructor parameters in a control structure and applying them
 * during first access via aggdata().
 *
 * @tparam T aggregate type
 * @tparam Args Arguments to the constructor of T
 */
template <typename T, typename... Args>
constexpr inline auto construct_aggregate = +[](T *target, void *control_ptr) {
  using control_t = aggregate_control<T, Args...>;
  static_assert(!over_aligned<control_t>,
                "Over-aligned constructor arguments are not supported.");
  static_assert(std::is_nothrow_constructible_v<T, Args...>,
                "Aggregate type must be nothrow constructible with provided arguments.");

  auto *control = pointer_cast<control_t>(control_ptr);
  return std::apply(
      [target](auto &&...args) {
        std::construct_at(target, std::forward<decltype(args)>(args)...);
      },
      control->constructor_args);
};

template <typename T>
struct lazy_initialised {
  T instance;
  // we rely on sqlite3_aggregate_context memset zero when first called
  bool is_initialised;
};
} // namespace detail

template <typename T>
T *aggdata(context_raw *ctx) {
  using wrapper_t = detail::lazy_initialised<T>;
  auto *storage = pointer_cast<wrapper_t>(
      sqlite3_aggregate_context(ctx, detail::storage_size_v<wrapper_t>));
  if (!storage)
    return nullptr;

  if (storage->is_initialised)
    return &storage->instance;

  // Perform lazy initialisation
  auto *control = userdata<detail::aggregate_control_base<T>>(ctx);
  if (!control) [[unlikely]]
    // ! It's a bug if control flow enters here.
    // ! This indicates user ignored error::nomem returned by create_aggregate().
    return nullptr;

  // TODO: Relax nothrow requirement of Ctor
  control->initialise(&storage->instance, void_cast(control));
  storage->is_initialised = true;
  return &storage->instance;
}

template <simple T>
T *aggdata(context_raw *ctx) {
  return pointer_cast<T>(sqlite3_aggregate_context(ctx, detail::storage_size_v<T>));
}

} // namespace ju::sqlite
