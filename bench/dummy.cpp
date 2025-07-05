#include <benchmark/benchmark.h>

#include "utils.hpp"

static void bench0(benchmark::State &state) {
  for (auto _ : state) {
    auto d = bu::make_unif_vect<int>(100, 0, 100);
  }
}
BENCHMARK(bench0);

BENCHMARK_MAIN();
