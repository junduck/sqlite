#pragma once

#include "tag.hpp"

namespace ju::sqlite {
namespace detail {
template <size_t N>
struct typeid_t {
  char data[N + 1]{};
  constexpr typeid_t(char const *s) noexcept { std::copy(s, s + N, data); }
  constexpr operator char const *() const noexcept { return data; }
};
template <size_t N>
typeid_t(char const (&)[N]) -> typeid_t<N - 1>;

template <typename T>
concept over_aligned = alignof(T) > alignof(std::max_align_t);

template <typename T>
struct storage_size {
  constexpr static size_t value_raw = sizeof(T);
};

template <over_aligned T>
struct storage_size<T> {
  constexpr static size_t value_raw = sizeof(T) + alignof(T) - 1;
};

template <typename T>
constexpr size_t storage_size_v = storage_size<T>::value_raw;

template <typename T>
T *pointer_cast(void *storage) {
  return static_cast<T *>(storage);
}

template <typename T>
T *pointer_cast(void *storage)
  requires(over_aligned<T>)
{
  auto const addr = reinterpret_cast<std::uintptr_t>(storage);
  auto const aligned = (addr + alignof(T) - 1u) & ~(alignof(T) - 1u);
  return reinterpret_cast<T *>(aligned);
}

template <typename T>
T *pointer_cast(void *storage)
  requires(std::is_function_v<T>)
{
  // ! This may be a bad idea
  return reinterpret_cast<T *>(storage);
}

template <typename T>
void *void_cast(T *p) {
  return static_cast<void *>(p);
}

template <typename T>
void *void_cast(T *p)
  requires(std::is_function_v<T>)
{
  // ! This may be a bad idea
  return reinterpret_cast<void *>(p);
}

/*

Regarding to casting function pointers, maybe we should add an extra indirection to avoid
UB?

struct function_ptr_wrapper {
  using storage_type = void (*)();
  storage_type fn;
};

then we can safely cast fn to original function type without UB
reinterpret_cast<original_func>(static_cast<function_ptr_wrapper>(p)->fn)

*/

template <typename T>
constexpr inline destructor_type_raw managed_deleter = +[](void *storage) {
  auto p = pointer_cast<T>(storage);
  std::destroy_at(p);
  sqlite3_free(storage);
};
} // namespace detail

using detail::pointer_cast;
using detail::void_cast;

/*
 * No-op deleter for pointer_t
 *
 * When user wants to explicitly manage the lifetime of a pointer_t,
 * they can use this deleter to avoid SQLite3 cleanup.
 */
constexpr inline destructor_type_raw noop_deleter = nullptr;

template <typename T, detail::typeid_t TypeId, destructor_type_raw D = noop_deleter>
class pointer_t {
  void *ptr{};
  constexpr static destructor_type_raw d = D;

  friend auto tag_invoke(tag::deleter_tag, pointer_t) noexcept { return d; }

  friend auto tag_invoke(tag::cast_tag, type_t<pointer_t>, value_raw *val) {
    return pointer_t{sqlite3_value_pointer(val, TypeId)};
  }

  friend auto tag_invoke(tag::bind_tag, stmt_raw *st, int idx, pointer_t p) {
    auto rc = sqlite3_bind_pointer(st, idx, p, type_id, d);
    return to_error(rc);
  }

  friend auto tag_invoke(tag::bind_tag, context_raw *ctx, pointer_t p) {
    sqlite3_result_pointer(ctx, p, type_id, d);
  }

public:
  using pointer = T *;
  using element_type = T;
  constexpr static auto type_id = TypeId;

  pointer_t() noexcept = default;
  explicit pointer_t(void *p) noexcept : ptr{p} {}
  explicit pointer_t(T *p) noexcept : ptr{static_cast<void *>(p)} {}

  T *get() const noexcept { return pointer_cast<T>(ptr); }
  operator void *() const noexcept { return ptr; }
  T &operator*() const noexcept { return *get(); }
  T *operator->() const noexcept { return get(); }

  explicit operator bool() const noexcept { return ptr != nullptr; }
  bool operator==(std::nullptr_t) const noexcept { return ptr == nullptr; }
  bool operator==(pointer_t const &other) const noexcept { return ptr == other.ptr; }
};

template <typename T>
using managed_ptr = pointer_t<T, "$ptr$", detail::managed_deleter<T>>;

/**
 * @brief Create a managed pointer to an object allocated with SQLite3's memory functions.
 *
 * Allocates memory for an object of type T using SQLite3's memory allocator,
 * then constructs the object in that memory. The returned managed pointer must
 * be passed to a SQLite3 function (automatically converted to a void*), and
 * the xDestroy parameter must be set to get_deleter(p) to ensure proper cleanup
 * with SQLite3's memory management. Failing to do so will result in a memory leak.
 *
 * @tparam T The element type to construct.
 * @tparam Args Constructor argument types.
 * @param args Constructor arguments for T.
 * @return managed_ptr<T> Managed pointer to the constructed object.
 */
template <typename T, typename... Args>
managed_ptr<T> make_managed(Args &&...args) {
  managed_ptr<T> p{sqlite3_malloc64(detail::storage_size_v<T>)};
  if (p)
    try {
      std::construct_at(p.get(), static_cast<Args &&>(args)...);
    } catch (...) {
      sqlite3_free(p);
      return {};
    }

  return p;
}
} // namespace ju::sqlite
