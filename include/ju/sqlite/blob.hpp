#pragma once

#include "sqlite3.h"

#include <array>
#include <ranges>
#include <string>
#include <string_view>

namespace ju::sqlite {

namespace detail {
template <typename T, typename U>
concept view_of = requires(T t) {
  { std::ranges::size(t) } -> std::convertible_to<std::size_t>;
  { std::ranges::data(t) } -> std::convertible_to<U const *>;
} && std::constructible_from<std::decay_t<T>, U const *, size_t>;
} // namespace detail

template <typename T>
concept text_like = detail::view_of<T, char>;

template <typename T>
concept blob_like = detail::view_of<T, unsigned char>;

// default concrete view type

using text_view = std::string_view;
using blob_view = std::basic_string_view<unsigned char>;

struct uuid_array : public std::array<unsigned char, 16> {
  using base = std::array<unsigned char, 16>;
  using base::base;

  uuid_array(unsigned char const *raw, size_t) noexcept {
    std::copy(raw, raw + size(), data());
  }

  explicit operator bool() const noexcept { return *this != uuid_array{}; }

  operator std::string() const {
    std::string val;
    val.reserve(size() * 2);
    for (size_t i = 0; i < size(); ++i) {
      auto byte = data()[i];
      val.push_back("0123456789abcdef"[byte >> 4]);
      val.push_back("0123456789abcdef"[byte & 0xF]);
    }
    return val;
  }
};
static_assert(blob_like<uuid_array>);

} // namespace ju::sqlite
