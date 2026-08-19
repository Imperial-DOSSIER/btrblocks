#pragma once
// -------------------------------------------------------------------------------------
#include <cstdint>
#include <random>
#include <vector>
// -------------------------------------------------------------------------------------
// Snowflake-style identifier generators.
//
// A snowflake packs several semantic fields into one integer, each with its own
// statistics: a timestamp that advances slowly and monotonically, a shard id
// drawn from a handful of values, and a sequence that cycles. That is exactly
// the shape SubIntSplit targets and no single scheme serves well, which is why
// it is the primary benchmark input.
//
// Reproducible from a seed, generated in memory, no I/O and no dependencies.
// -------------------------------------------------------------------------------------
namespace btrblocks::subintsplit_bench {
// -------------------------------------------------------------------------------------
struct SnowflakeConfig {
  int timestamp_bits;
  int shard_bits;
  int sequence_bits;
  // Distinct shard ids in play. Fewer than the field can hold is realistic --
  // a deployment sizes the field for growth -- and it is what makes the shard
  // section compress far better than its width suggests.
  uint64_t shard_count;
  uint64_t timestamp_origin;
};
// -------------------------------------------------------------------------------------
// The Instagram layout: 41-bit millisecond timestamp, 13-bit shard, 10-bit
// per-millisecond sequence, sign bit unused.
inline SnowflakeConfig instagramSnowflake64() {
  SnowflakeConfig cfg;
  cfg.timestamp_bits = 41;
  cfg.shard_bits = 13;
  cfg.sequence_bits = 10;
  cfg.shard_count = 16;
  cfg.timestamp_origin = 1700000000000ull;
  return cfg;
}
// -------------------------------------------------------------------------------------
// A 32-bit analogue: 21-bit timestamp, 7-bit shard, 4-bit sequence.
inline SnowflakeConfig instagramSnowflake32() {
  SnowflakeConfig cfg;
  cfg.timestamp_bits = 21;
  cfg.shard_bits = 7;
  cfg.sequence_bits = 4;
  cfg.shard_count = 5;
  cfg.timestamp_origin = 100000;
  return cfg;
}
// -------------------------------------------------------------------------------------
template <typename T>
std::vector<T> generateSnowflakes(std::size_t count, const SnowflakeConfig& cfg, uint32_t seed) {
  std::mt19937_64 gen(seed);
  std::vector<T> values;
  values.reserve(count);

  const uint64_t sequence_limit = uint64_t{1} << cfg.sequence_bits;
  uint64_t timestamp = cfg.timestamp_origin;
  uint64_t sequence = 0;

  for (std::size_t i = 0; i < count; i++) {
    const uint64_t shard = gen() % cfg.shard_count;
    const uint64_t value = (timestamp << (cfg.shard_bits + cfg.sequence_bits)) |
                           (shard << cfg.sequence_bits) | sequence;
    values.push_back(static_cast<T>(value));
    // The sequence exhausts within a millisecond, then the clock ticks -- which
    // is what gives the timestamp field its long runs.
    if (++sequence == sequence_limit) {
      sequence = 0;
      timestamp++;
    }
  }
  return values;
}
// -------------------------------------------------------------------------------------
// Uniformly random values, as a control: no bit-range structure at all, so
// splitting should not help and the planner should decline to split.
template <typename T>
std::vector<T> generateUniform(std::size_t count, uint32_t seed) {
  std::mt19937_64 gen(seed);
  std::vector<T> values;
  values.reserve(count);
  for (std::size_t i = 0; i < count; i++) {
    values.push_back(static_cast<T>(gen()));
  }
  return values;
}
// -------------------------------------------------------------------------------------
// A monotonically increasing sequence with small gaps, the case bit-packing
// after a frame-of-reference already handles well.
template <typename T>
std::vector<T> generateIncreasing(std::size_t count, uint32_t seed) {
  std::mt19937_64 gen(seed);
  std::vector<T> values;
  values.reserve(count);
  uint64_t current = 1000000;
  for (std::size_t i = 0; i < count; i++) {
    values.push_back(static_cast<T>(current));
    current += 1 + (gen() % 8);
  }
  return values;
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::subintsplit_bench
// -------------------------------------------------------------------------------------
