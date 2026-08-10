// -------------------------------------------------------------------------------------
// Round-trip and safety tests for the SubIntSplit scheme.
//
// Built on in-memory data rather than the test-dataset fixtures, so they run
// without first building and running test_dataset_generator.
// -------------------------------------------------------------------------------------
#include "TestHelper.hpp"
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "common/SIMD.hpp"
#include "compression/Datablock.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/SchemePool.hpp"
#include "scheme/integer/subintsplit/Plan.hpp"
#include "scheme/integer/subintsplit/SubIntSplitCore.hpp"
#include "storage/Relation.hpp"
// -------------------------------------------------------------------------------------
#include "gtest/gtest.h"
// -------------------------------------------------------------------------------------
#include <cstdint>
#include <limits>
#include <random>
#include <vector>
// -------------------------------------------------------------------------------------
using namespace btrblocks;
using namespace btrblocks::subintsplit;
// -------------------------------------------------------------------------------------
namespace {
// -------------------------------------------------------------------------------------
// A 32-bit snowflake-shaped value: 21-bit timestamp, 7-bit machine, 4-bit
// sequence -- the int32 layout the research harness uses.
std::vector<INTEGER> makeSnowflake32(std::size_t count, uint32_t seed = 1) {
  std::mt19937 gen(seed);
  std::vector<INTEGER> values;
  values.reserve(count);
  uint32_t timestamp = 100000;
  uint32_t sequence = 0;
  for (std::size_t i = 0; i < count; i++) {
    const uint32_t machine = gen() % 5;
    values.push_back(static_cast<INTEGER>((timestamp << 11) | (machine << 4) | sequence));
    if (++sequence == 16) {
      sequence = 0;
      timestamp++;
    }
  }
  return values;
}
// -------------------------------------------------------------------------------------
// Round-trips `data` through the scheme directly and asserts every value comes
// back. Returns the encoded size.
u32 roundTrip(const std::vector<INTEGER>& data, u8 cascade_level = 3) {
  const auto tuple_count = static_cast<u32>(data.size());
  const std::vector<BITMAP> bitmap(std::max<std::size_t>(tuple_count, 1), 1);

  auto& scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(CB(IntegerSchemeType::SUB_INT_SPLIT));
  auto stats = SInteger32Stats::generateStats(data.data(), bitmap.data(), tuple_count);
  auto compressed =
      makeBytesArray(static_cast<std::size_t>(tuple_count) * sizeof(INTEGER) * 12 + 4096);
  const u32 size =
      scheme.compress(data.data(), bitmap.data(), compressed.get(), stats, cascade_level);

  BitmapWrapper nullmap(nullptr, BitmapType::ALLONES, tuple_count);
  std::vector<INTEGER> decoded(tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER));
  scheme.decompress(decoded.data(), &nullmap, compressed.get(), tuple_count, 0);

  for (u32 i = 0; i < tuple_count; i++) {
    EXPECT_EQ(decoded[i], data[i]) << "row " << i;
    if (decoded[i] != data[i]) {
      break;
    }
  }
  return size;
}
// -------------------------------------------------------------------------------------
}  // namespace
// -------------------------------------------------------------------------------------
TEST(SubIntSplit, Begin) {
  BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
  BtrBlocksConfig::get().integers.schemes.enable(IntegerSchemeType::SUB_INT_SPLIT);
  BtrBlocksConfig::get().doubles.schemes = defaultDoubleSchemes();
  BtrBlocksConfig::get().strings.schemes = defaultStringSchemes();
  SchemePool::refresh();
}
// -------------------------------------------------------------------------------------
// No section may itself be compressed with SubIntSplit. Beyond being
// meaningless, it recurses: the picker asks each candidate to estimate itself,
// which re-enters encode() and clobbers the shared scratch buffers, silently
// corrupting every section but the last.
//
// The subtlety this pins is the entry depth. Reaching compress() through the
// picker starts one cascade level deeper than calling it directly, so a guard
// written for the former alone leaves the latter -- which is how the benchmark
// driver and these tests call it -- unprotected.
TEST(SubIntSplit, SectionsAreNeverSubIntSplit) {
  const u32 count = 16384;
  const auto data = makeSnowflake32(count);
  const std::vector<BITMAP> bitmap(count, 1);
  auto stats = SInteger32Stats::generateStats(data.data(), bitmap.data(), count);

  auto& scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(CB(IntegerSchemeType::SUB_INT_SPLIT));
  auto compressed = makeBytesArray(static_cast<std::size_t>(count) * sizeof(INTEGER) * 12 + 4096);

  // Called at the shallowest possible depth, which is the unguarded case.
  ASSERT_EQ(ThreadCache::get().compression_level, 0);
  scheme.compress(data.data(), bitmap.data(), compressed.get(), stats, 3);

  const auto* header = reinterpret_cast<const SubIntSplitHeader*>(compressed.get());
  const auto* descriptors = reinterpret_cast<const SectionDescriptor*>(header->data);
  ASSERT_GT(header->section_count, 1) << "structured input should have been split";
  for (u8 s = 0; s < header->section_count; s++) {
    ASSERT_NE(descriptors[s].scheme_code, CB(IntegerSchemeType::SUB_INT_SPLIT))
        << "section " << static_cast<int>(s) << " recursed into SubIntSplit";
  }

  // The scope guard must leave the cascade depth exactly as it found it.
  ASSERT_EQ(ThreadCache::get().compression_level, 0);
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplit, RoundTripSnowflake) {
  roundTrip(makeSnowflake32(65536));
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplit, RoundTripEdgeSizes) {
  for (const u32 count : {0u, 1u, 7u, 127u, 128u, 129u, 640u, 65536u}) {
    SCOPED_TRACE("tuple_count = " + std::to_string(count));
    roundTrip(makeSnowflake32(count));
  }
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplit, RoundTripValueEdges) {
  const u32 count = 4096;

  {
    SCOPED_TRACE("all zero");
    roundTrip(std::vector<INTEGER>(count, 0));
  }
  {
    SCOPED_TRACE("all same");
    roundTrip(std::vector<INTEGER>(count, 0x5A5A5A5A));
  }
  {
    // Every bit set: the top section spans the sign bit, which is the case that
    // breaks any scheme doing signed arithmetic on section values.
    SCOPED_TRACE("all bits set");
    roundTrip(std::vector<INTEGER>(count, -1));
  }
  {
    SCOPED_TRACE("negatives");
    std::vector<INTEGER> data(count);
    for (u32 i = 0; i < count; i++) {
      data[i] = -static_cast<INTEGER>(i) - 1;
    }
    roundTrip(data);
  }
  {
    SCOPED_TRACE("extremes");
    std::vector<INTEGER> data(count);
    for (u32 i = 0; i < count; i++) {
      data[i] =
          (i % 2 == 0) ? std::numeric_limits<INTEGER>::min() : std::numeric_limits<INTEGER>::max();
    }
    roundTrip(data);
  }
  {
    SCOPED_TRACE("uniform random");
    std::mt19937 gen(17);
    std::vector<INTEGER> data(count);
    for (auto& value : data) {
      value = static_cast<INTEGER>(gen());
    }
    roundTrip(data);
  }
}
// -------------------------------------------------------------------------------------
// The encoded size must never exceed the raw size by more than the header, no
// matter how incompressible the input. Every section costs a full INTEGER per
// value before its sub-scheme runs, so without the fallback a multi-section
// plan over random data would be several times larger than the input -- and it
// would be written into a shared datablock buffer sized on the assumption that
// it is not.
TEST(SubIntSplit, EncodedSizeNeverExceedsRaw) {
  std::mt19937 gen(23);
  const u32 count = 32768;
  std::vector<INTEGER> data(count);
  for (auto& value : data) {
    value = static_cast<INTEGER>(gen());
  }

  const u32 size = roundTrip(data);
  const u32 raw = count * sizeof(INTEGER);
  ASSERT_LE(size, raw + 64) << "encoded " << size << " vs raw " << raw;
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplit, TerminatesAtEveryCascadeDepth) {
  const auto data = makeSnowflake32(8192);
  for (u8 depth = 1; depth <= 4; depth++) {
    SCOPED_TRACE("max_cascade_depth = " + std::to_string(depth));
    roundTrip(data, depth);
  }
}
// -------------------------------------------------------------------------------------
// Nesting SubIntSplit inside itself is meaningless and would recurse through
// scheme estimation, so it must decline to be considered below the top level.
TEST(SubIntSplit, DeclinesWhenNested) {
  const auto data = makeSnowflake32(1024);
  const std::vector<BITMAP> bitmap(data.size(), 1);
  auto stats =
      SInteger32Stats::generateStats(data.data(), bitmap.data(), static_cast<u32>(data.size()));

  auto& scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(CB(IntegerSchemeType::SUB_INT_SPLIT));

  ThreadCache::get().compression_level = 2;
  ASSERT_EQ(scheme.expectedCompressionRatio(stats, 3), 0);
  ThreadCache::get().compression_level = 0;

  // And it declines when there is no cascade budget left for its sections.
  ASSERT_EQ(scheme.expectedCompressionRatio(stats, 1), 0);
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplit, ForcedBoundariesAreUsed) {
  const auto data = makeSnowflake32(16384);

  std::vector<SegmentPlan> boundaries;
  ASSERT_TRUE(parseSplitBoundaries("0-15;16-31", 32, boundaries));

  EnforceSplitBoundaries enforcer(boundaries);
  roundTrip(data);

  // And the encoded form really does carry the requested split.
  const std::vector<BITMAP> bitmap(data.size(), 1);
  auto stats =
      SInteger32Stats::generateStats(data.data(), bitmap.data(), static_cast<u32>(data.size()));
  auto& scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(CB(IntegerSchemeType::SUB_INT_SPLIT));
  auto compressed = makeBytesArray(data.size() * sizeof(INTEGER) * 12 + 4096);
  scheme.compress(data.data(), bitmap.data(), compressed.get(), stats, 3);

  ASSERT_EQ(SubIntSplitCore<u32>::sectionCount(compressed.get()), 2);
  const auto description = scheme.fullDescription(compressed.get());
  ASSERT_NE(description.find("0-15;16-31"), std::string::npos) << description;
}
// -------------------------------------------------------------------------------------
// The header is on-disk format, so an unrecognised version must be refused
// rather than misparsed into a wild section count and out-of-range offsets.
TEST(SubIntSplit, RejectsUnknownFormatVersion) {
  const auto data = makeSnowflake32(1024);
  const std::vector<BITMAP> bitmap(data.size(), 1);
  auto stats =
      SInteger32Stats::generateStats(data.data(), bitmap.data(), static_cast<u32>(data.size()));
  auto& scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(CB(IntegerSchemeType::SUB_INT_SPLIT));
  auto compressed = makeBytesArray(data.size() * sizeof(INTEGER) * 12 + 4096);
  scheme.compress(data.data(), bitmap.data(), compressed.get(), stats, 3);

  reinterpret_cast<SubIntSplitHeader*>(compressed.get())->format_version = 99;

  BitmapWrapper nullmap(nullptr, BitmapType::ALLONES, static_cast<u32>(data.size()));
  std::vector<INTEGER> decoded(data.size() + SIMD_EXTRA_ELEMENTS(INTEGER));
  ASSERT_THROW(scheme.decompress(decoded.data(), &nullmap, compressed.get(),
                                 static_cast<u32>(data.size()), 0),
               Generic_Exception);
}
// -------------------------------------------------------------------------------------
// End to end through Relation and Datablock, which is the path real data takes.
TEST(SubIntSplit, RoundTripThroughDatablock) {
  EnforceScheme<IntegerSchemeType> enforcer(IntegerSchemeType::SUB_INT_SPLIT);

  const u32 count = 40000;
  const auto data = makeSnowflake32(count, 5);

  Relation relation;
  {
    Vector<INTEGER> column(count);
    for (u32 i = 0; i < count; i++) {
      column[i] = data[i];
    }
    relation.addColumn({"ids", std::move(column)});
  }

  Datablock datablock(relation);
  TestHelper::CheckRelationCompression(relation, datablock, {CB(IntegerSchemeType::SUB_INT_SPLIT)});
}
// -------------------------------------------------------------------------------------
TEST(SubIntSplit, End) {
  BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
  SchemePool::refresh();
}
// -------------------------------------------------------------------------------------
