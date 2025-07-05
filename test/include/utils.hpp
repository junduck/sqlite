#pragma once

#include <random>
#include <type_traits>
#include <vector>

namespace tu {
template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template <arithmetic T>
auto make_unif_vect(size_t n, T min, T max) {
  std::vector<T> data;
  data.reserve(n);

  std::random_device rd;
  std::mt19937 gen{rd()};
  if constexpr (std::floating_point<T>) {
    std::uniform_real_distribution<T> dist(min, max);
    for (size_t i = 0; i < n; ++i)
      data.push_back(dist(gen));
  } else {
    std::uniform_int_distribution<T> dist(min, max);
    for (size_t i = 0; i < n; ++i)
      data.push_back(dist(gen));
  }

  return data;
}
} // namespace tu
