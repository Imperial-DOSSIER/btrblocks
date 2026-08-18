// -------------------------------------------------------------------------------------
#include "CompressionScheme64.hpp"
#include "btrblocks.hpp"
#include "cache/ThreadCache.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
double Integer64Scheme::expectedCompressionRatio(SInteger64Stats& stats,
                                                  u8 allowed_cascading_level) {
  auto& cfg = BtrBlocksConfig::get();
  auto dest = makeBytesArray(CS(cfg.sample_size) * cfg.sample_count * sizeof(BIGINT) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(cfg.sample_count, cfg.sample_size);
    SInteger64Stats c_stats = SInteger64Stats::generateStats(
        std::get<0>(sample).data(), std::get<1>(sample).data(), std::get<0>(sample).size());
    total_before += c_stats.total_size;
    total_after += compress(std::get<0>(sample).data(), std::get<1>(sample).data(), dest.get(),
                            c_stats, allowed_cascading_level);
  }
  ThreadCache::get().estimation_level--;
  return CD(total_before) / CD(total_after);
}
// -------------------------------------------------------------------------------------
namespace {
// Own scratch stack, distinct from IntegerScheme's INTEGER-typed one in
// CompressionScheme.cpp -- see that file's comment for why this pattern
// (thread_local, indexed by cascade level) is used.
thread_local std::vector<std::vector<BIGINT>> random_access_scratch64;
}  // namespace
// -------------------------------------------------------------------------------------
void Integer64Scheme::gather(BIGINT* dest,
                             const u8* src,
                             BitmapWrapper* nullmap,
                             u32 tuple_count,
                             const u32* positions,
                             u32 position_count,
                             u32 level) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  BIGINT* scratch =
      get_level_data(random_access_scratch64, tuple_count + SIMD_EXTRA_ELEMENTS(BIGINT), level);
  this->decompress(scratch, nullmap, src, tuple_count, level);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = scratch[positions[i]];
  }
}
// -------------------------------------------------------------------------------------
BIGINT Integer64Scheme::lookupAt(const u8* src,
                                 BitmapWrapper* nullmap,
                                 u32 tuple_count,
                                 u32 position,
                                 u32 level) {
  BIGINT result = 0;
  this->gather(&result, src, nullmap, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
string ConvertSchemeTypeToString(Integer64SchemeType type) {
  switch (type) {
    case Integer64SchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    case Integer64SchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case Integer64SchemeType::DICT:
      return "DICT";
    case Integer64SchemeType::RLE:
      return "RLE";
    case Integer64SchemeType::PFOR:
      return "PFOR";
    case Integer64SchemeType::BP:
      return "BP";
    case Integer64SchemeType::SUB_INT_SPLIT:
      return "SUB_INT_SPLIT";
    case Integer64SchemeType::FREQUENCY:
      return "FREQUENCY";
    case Integer64SchemeType::FOR:
      return "FOR";
    case Integer64SchemeType::TRUNCATION:
      return "TRUNCATION";
    case Integer64SchemeType::FIXED_DICTIONARY:
      return "FIXED_DICTIONARY";
    default:
      throw Generic_Exception("Unknown Integer64SchemeType");
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
