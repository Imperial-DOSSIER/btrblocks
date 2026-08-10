// -------------------------------------------------------------------------------------
// Unit tests for the SubIntSplit selection layer.
//
// The planner has no dependency on schemes, the scheme pool or the wire format,
// so these tests drive it directly on synthetic bit-field layouts and need no
// BtrBlocks setup beyond the configuration singletons.
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "scheme/integer/subintsplit/CostModels.hpp"
#include "scheme/integer/subintsplit/Metrics.hpp"
#include "scheme/integer/subintsplit/Plan.hpp"
#include "scheme/integer/subintsplit/Sampler.hpp"
#include "scheme/integer/subintsplit/Selector.hpp"
// -------------------------------------------------------------------------------------
#include "gtest/gtest.h"
// -------------------------------------------------------------------------------------
#include <cstdint>
#include <random>
#include <vector>
// -------------------------------------------------------------------------------------
using namespace btrblocks;
using namespace btrblocks::subintsplit;
// -------------------------------------------------------------------------------------
namespace {
// -------------------------------------------------------------------------------------
// A 64-bit snowflake: 1 zero sign bit, a slowly-advancing 41-bit timestamp, a
// low-cardinality 13-bit machine id, and a fast-cycling 10-bit sequence.
std::vector<uint64_t> makeSnowflakes(std::size_t count, uint32_t seed = 1) {
  std::mt19937_64 gen(seed);
  std::vector<uint64_t> values;
  values.reserve(count);
  uint64_t timestamp = 1'700'000'000'000ull;
  uint64_t sequence = 0;
  for (std::size_t i = 0; i < count; i++) {
    const uint64_t machine = gen() % 8;  // only 8 distinct machines
    values.push_back((timestamp << 23) | (machine << 10) | sequence);
    if (++sequence == 1024) {
      sequence = 0;
      timestamp++;
    }
  }
  return values;
}
// -------------------------------------------------------------------------------------
bool coversExactly(const SplitPlan& plan, int totalBits) {
  if (plan.segments.empty()) {
    return false;
  }
  int expected = 0;
  for (const auto& segment : plan.segments) {
    if (segment.bitStart != expected || segment.bitEnd < segment.bitStart) {
      return false;
    }
    expected = segment.bitEnd + 1;
  }
  return expected == totalBits;
}
// -------------------------------------------------------------------------------------
}  // namespace
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, MetricsOnKnownValues) {
  MetricCollector collector;
  // Two distinct values, three runs, dominant value appears 4 of 6 times.
  const std::vector<uint64_t> values{1, 1, 1, 2, 2, 1};
  const auto metrics = collector.compute(values, 4, static_cast<MetricFlags>(MetricFlag::All));

  ASSERT_EQ(metrics.min, 1u);
  ASSERT_EQ(metrics.max, 2u);
  ASSERT_EQ(metrics.range, 1u);
  ASSERT_EQ(metrics.uniqueCount, 2u);
  ASSERT_EQ(metrics.dominantCount, 4u);
  ASSERT_EQ(metrics.runCount, 3u);
  ASSERT_DOUBLE_EQ(metrics.avgRunLength, 2.0);
}
// -------------------------------------------------------------------------------------
// The direct-count and hash-map counting paths must agree. The switch is at
// kDirectCountBits, so this drives a range on each side of it.
TEST(SubIntSplitSelector, CountingPathsAgree) {
  std::mt19937_64 gen(5);
  std::vector<uint64_t> values;
  values.reserve(4000);
  for (std::size_t i = 0; i < 4000; i++) {
    values.push_back(gen() % 1000);
  }

  MetricCollector collector;
  const auto narrow = collector.compute(values, MetricCollector::kDirectCountBits,
                                        static_cast<MetricFlags>(MetricFlag::All));
  const auto wide = collector.compute(values, MetricCollector::kDirectCountBits + 1,
                                      static_cast<MetricFlags>(MetricFlag::All));

  ASSERT_EQ(narrow.uniqueCount, wide.uniqueCount);
  ASSERT_EQ(narrow.dominantCount, wide.dominantCount);
  ASSERT_EQ(narrow.min, wide.min);
  ASSERT_EQ(narrow.max, wide.max);
}
// -------------------------------------------------------------------------------------
// The collector is reused across the whole bit-range grid, so a stale counter
// array would silently inflate later ranges' cardinality.
TEST(SubIntSplitSelector, CollectorIsReusableAcrossCalls) {
  MetricCollector collector;
  const std::vector<uint64_t> first{1, 2, 3, 4, 5, 6, 7, 8};
  const std::vector<uint64_t> second{9, 9, 9, 9};

  collector.compute(first, 8, static_cast<MetricFlags>(MetricFlag::All));
  const auto metrics = collector.compute(second, 8, static_cast<MetricFlags>(MetricFlag::All));

  ASSERT_EQ(metrics.uniqueCount, 1u);
  ASSERT_EQ(metrics.dominantCount, 4u);
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, SamplerDrawsBlocks) {
  std::vector<int32_t> values(100000);
  for (std::size_t i = 0; i < values.size(); i++) {
    values[i] = static_cast<int32_t>(i);
  }

  SamplerConfig cfg;
  cfg.maxSamples = 2048;
  cfg.blockSize = 128;
  std::vector<uint64_t> samples;
  sampleIntoU64(values.data(), values.size(), nullptr, samples, cfg);

  ASSERT_EQ(samples.size(), 2048u);
  // Within a block the values are consecutive, which is exactly the local
  // structure block sampling exists to preserve.
  ASSERT_EQ(samples[1], samples[0] + 1);
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, SamplerSkipsNulls) {
  std::vector<int32_t> values(1000, 7);
  std::vector<BITMAP> nullmap(1000, 1);
  for (std::size_t i = 0; i < values.size(); i += 2) {
    values[i] = 999999;  // garbage in the null slots
    nullmap[i] = 0;
  }

  SamplerConfig cfg;
  cfg.maxSamples = 256;
  cfg.blockSize = 64;
  std::vector<uint64_t> samples;
  sampleIntoU64(values.data(), values.size(), nullmap.data(), samples, cfg);

  ASSERT_FALSE(samples.empty());
  for (const auto sample : samples) {
    ASSERT_EQ(sample, 7u) << "garbage from a null slot reached the sample";
  }
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, PlanCoversValueExactly) {
  const auto values = makeSnowflakes(20000);
  SamplerConfig samplerCfg;
  std::vector<uint64_t> samples;
  sampleIntoU64(values.data(), values.size(), nullptr, samples, samplerCfg);

  auto cfg = defaultSelectorConfig();
  const auto plan = selectSplits(samples, 64, values.size(), defaultCostModels(), cfg);

  ASSERT_TRUE(coversExactly(plan, 64)) << "segments must tile [0, 64)";
  ASSERT_LE(plan.segments.size(), static_cast<std::size_t>(cfg.maxSections));
  for (const auto& segment : plan.segments) {
    ASSERT_LE(segment.width(), cfg.maxSectionBits);
    ASSERT_GE(segment.width(), cfg.minSectionBits);
  }
}
// -------------------------------------------------------------------------------------
// The planner should find structure in a snowflake rather than leaving it whole:
// the sequence field cycles, the machine field is nearly constant and the
// timestamp barely moves, so one scheme cannot serve all three.
TEST(SubIntSplitSelector, PlanSplitsStructuredValues) {
  const auto values = makeSnowflakes(50000);
  SamplerConfig samplerCfg;
  std::vector<uint64_t> samples;
  sampleIntoU64(values.data(), values.size(), nullptr, samples, samplerCfg);

  const auto plan =
      selectSplits(samples, 64, values.size(), defaultCostModels(), defaultSelectorConfig());
  ASSERT_GT(plan.segments.size(), 1u) << "structured value was left unsplit";
}
// -------------------------------------------------------------------------------------
// Uniform noise has no bit-range structure to exploit, so splitting it should
// not look attractive. This is the counterpart to the test above: it checks the
// split penalty is actually doing its job.
TEST(SubIntSplitSelector, PlanKeepsUnstructuredValuesWhole) {
  std::mt19937_64 gen(11);
  std::vector<uint64_t> samples;
  samples.reserve(2048);
  for (std::size_t i = 0; i < 2048; i++) {
    samples.push_back(gen() & 0xFFFFFFFFull);
  }

  auto cfg = defaultSelectorConfig();
  cfg.maxSectionBits = 32;
  const auto plan = selectSplits(samples, 32, 1000000, defaultCostModels(), cfg);

  ASSERT_TRUE(coversExactly(plan, 32));
  ASSERT_EQ(plan.segments.size(), 1u) << "random noise should not be split";
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, PlanRespectsMaxSections) {
  const auto values = makeSnowflakes(20000);
  SamplerConfig samplerCfg;
  std::vector<uint64_t> samples;
  sampleIntoU64(values.data(), values.size(), nullptr, samples, samplerCfg);

  // Starts at 2: covering 64 bits with sections capped at 32 needs at least two
  // of them, so maxSections = 1 is infeasible and takes the fallback path
  // instead (see PlanFallsBackWhenInfeasible).
  for (int limit = 2; limit <= 6; limit++) {
    auto cfg = defaultSelectorConfig();
    cfg.maxSections = limit;
    const auto plan = selectSplits(samples, 64, values.size(), defaultCostModels(), cfg);
    ASSERT_TRUE(coversExactly(plan, 64)) << "limit " << limit;
    ASSERT_LE(plan.segments.size(), static_cast<std::size_t>(limit)) << "limit " << limit;
  }
}
// -------------------------------------------------------------------------------------
// maxSections = 1 cannot tile 64 bits when sections are capped at 32, so the DP
// has no solution at all. The fallback must still return a legal plan.
TEST(SubIntSplitSelector, PlanFallsBackWhenInfeasible) {
  const auto values = makeSnowflakes(4096);
  SamplerConfig samplerCfg;
  std::vector<uint64_t> samples;
  sampleIntoU64(values.data(), values.size(), nullptr, samples, samplerCfg);

  auto cfg = defaultSelectorConfig();
  cfg.maxSections = 1;
  cfg.maxSectionBits = 32;
  const auto plan = selectSplits(samples, 64, values.size(), defaultCostModels(), cfg);

  ASSERT_TRUE(coversExactly(plan, 64));
  for (const auto& segment : plan.segments) {
    ASSERT_LE(segment.width(), 32);
  }
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, PlanHandlesEmptySample) {
  const std::vector<uint64_t> samples;
  const auto plan = selectSplits(samples, 64, 0, defaultCostModels(), defaultSelectorConfig());
  ASSERT_TRUE(coversExactly(plan, 64));
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, BoundariesRoundTrip) {
  std::vector<SegmentPlan> parsed;
  ASSERT_TRUE(parseSplitBoundaries("0-9;10-22;23-63", 64, parsed));
  ASSERT_EQ(parsed.size(), 3u);
  ASSERT_EQ(parsed[0].bitStart, 0);
  ASSERT_EQ(parsed[0].bitEnd, 9);
  ASSERT_EQ(parsed[2].bitEnd, 63);
  ASSERT_EQ(serializeSplitBoundaries(parsed), "0-9;10-22;23-63");

  ASSERT_TRUE(parseSplitBoundaries("0-31", 32, parsed));
  ASSERT_EQ(parsed.size(), 1u);
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplitSelector, BoundariesRejectMalformed) {
  std::vector<SegmentPlan> parsed;
  ASSERT_FALSE(parseSplitBoundaries("", 64, parsed)) << "empty";
  ASSERT_FALSE(parseSplitBoundaries("0-9", 64, parsed)) << "does not cover the value";
  ASSERT_FALSE(parseSplitBoundaries("0-9;11-63", 64, parsed)) << "gap between ranges";
  ASSERT_FALSE(parseSplitBoundaries("0-9;5-63", 64, parsed)) << "overlapping ranges";
  ASSERT_FALSE(parseSplitBoundaries("0-9;10-64", 64, parsed)) << "runs past the value";
  ASSERT_FALSE(parseSplitBoundaries("9-0;1-63", 64, parsed)) << "descending range";
  ASSERT_FALSE(parseSplitBoundaries("0-x;1-63", 64, parsed)) << "non-numeric";
  ASSERT_FALSE(parseSplitBoundaries("0-9;;10-63", 64, parsed)) << "empty token";
}
// -------------------------------------------------------------------------------------
// The cap must drop to 31 when a sign-sensitive sub-scheme is enabled: FOR and
// the truncation schemes do signed arithmetic on section values, which
// overflows for a full-width section.
TEST(SubIntSplitSelector, MaxSectionBitsNarrowsForSignSensitiveSchemes) {
  const auto saved = BtrBlocksConfig::get().integers.schemes;

  BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
  ASSERT_EQ(effectiveMaxSectionBits(), 32);

  BtrBlocksConfig::get().integers.schemes.enable(IntegerSchemeType::FOR);
  ASSERT_EQ(effectiveMaxSectionBits(), 31);

  BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
  BtrBlocksConfig::get().integers.schemes.enable(IntegerSchemeType::TRUNCATION_8);
  ASSERT_EQ(effectiveMaxSectionBits(), 31);

  BtrBlocksConfig::get().integers.schemes = saved;
}
// -------------------------------------------------------------------------------------
