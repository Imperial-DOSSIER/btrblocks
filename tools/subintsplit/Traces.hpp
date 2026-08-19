#pragma once
// -------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>
// -------------------------------------------------------------------------------------
// Row-position traces for the gather benchmark.
//
// The choice of trace is not incidental here. A column is stored as chunks of
// block_size rows and a gather costs one full chunk decode per chunk it
// touches, so the number of distinct chunks touched -- not the number of rows
// requested -- is what a gather actually costs.
//
// For positions drawn uniformly over C chunks, the expected number touched is
// C * (1 - (1 - 1/C)^k), which saturates at C as soon as k is much larger than
// C. At the default block size a four-million-row column is only ~64 chunks, so
// any uniform gather of more than a few hundred rows touches every chunk and
// degenerates into a full column decode -- identically for every scheme, which
// measures nothing.
//
// Clustered traces are what make chunk locality visible, and they are also the
// realistic access pattern: a filtered scan yields runs of qualifying rows
// separated by gaps, not scattered singletons.
// -------------------------------------------------------------------------------------
namespace btrblocks::subintsplit_bench {
// -------------------------------------------------------------------------------------
// Positions drawn uniformly over [0, row_count). Sorted, which is how a scan
// would present them and which gives the bucketing its best case.
inline std::vector<uint32_t> uniformTrace(uint32_t row_count, uint32_t count, uint32_t seed) {
  std::mt19937 gen(seed);
  std::vector<uint32_t> positions;
  positions.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    positions.push_back(gen() % row_count);
  }
  std::sort(positions.begin(), positions.end());
  return positions;
}
// -------------------------------------------------------------------------------------
// Runs of consecutive rows separated by gaps, both geometrically distributed.
// `selectivity` is the fraction of rows selected overall and `mean_run` the
// mean length of a run, so the mean gap is mean_run * (1/selectivity - 1).
//
// This is the shape a filtered table scan produces, and unlike a uniform trace
// it leaves whole chunks untouched at low selectivity.
inline std::vector<uint32_t> clusteredTrace(uint32_t row_count,
                                            double selectivity,
                                            double mean_run,
                                            uint32_t seed) {
  std::mt19937 gen(seed);
  const double mean_gap = mean_run * (1.0 / selectivity - 1.0);
  std::geometric_distribution<uint32_t> run_dist(1.0 / std::max(1.0, mean_run));
  std::geometric_distribution<uint32_t> gap_dist(1.0 / std::max(1.0, mean_gap));

  std::vector<uint32_t> positions;
  positions.reserve(static_cast<std::size_t>(row_count * selectivity) + 1);

  uint32_t row = gap_dist(gen);
  while (row < row_count) {
    const uint32_t run = 1 + run_dist(gen);
    const uint32_t end = std::min(row + run, row_count);
    for (uint32_t i = row; i < end; i++) {
      positions.push_back(i);
    }
    row = end + 1 + gap_dist(gen);
  }
  return positions;
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::subintsplit_bench
// -------------------------------------------------------------------------------------
