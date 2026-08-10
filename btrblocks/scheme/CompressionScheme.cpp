// -------------------------------------------------------------------------------------
#include "CompressionScheme.hpp"
#include "btrblocks.hpp"
#include "cache/ThreadCache.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
double DoubleScheme::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  auto& cfg = BtrBlocksConfig::get();
  auto dest = makeBytesArray(CS(cfg.sample_size) * cfg.sample_count * sizeof(DOUBLE) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(cfg.sample_count, cfg.sample_size);
    DoubleStats c_stats = DoubleStats::generateStats(
        std::get<0>(sample).data(), std::get<1>(sample).data(), std::get<0>(sample).size());
    total_before += c_stats.total_size;
    total_after += compress(std::get<0>(sample).data(), std::get<1>(sample).data(), dest.get(),
                            c_stats, allowed_cascading_level);
  }
  ThreadCache::get().estimation_level--;
  return CD(total_before) / CD(total_after);
}
// -------------------------------------------------------------------------------------
double IntegerScheme::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  auto& cfg = BtrBlocksConfig::get();
  auto dest = makeBytesArray(CS(cfg.sample_size) * cfg.sample_count * sizeof(INTEGER) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(cfg.sample_count, cfg.sample_size);
    SInteger32Stats c_stats = SInteger32Stats::generateStats(
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
// Scratch stack for the default random-access implementations, indexed by
// cascade level like the scratch buffers in the templated schemes. A distinct
// vector object, so a scheme's own get_level_data() at the same level is
// untouched while its gather() runs.
thread_local std::vector<std::vector<INTEGER>> random_access_scratch;
}  // namespace
// -------------------------------------------------------------------------------------
void IntegerScheme::gather(INTEGER* dest,
                           const u8* src,
                           BitmapWrapper* nullmap,
                           u32 tuple_count,
                           const u32* positions,
                           u32 position_count,
                           u32 level) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  // The SIMD slack is mandatory, not defensive: TRLE<INTEGER>::decompressColumn
  // stores whole 256-bit vectors and overruns the logical end of the buffer.
  INTEGER* scratch =
      get_level_data(random_access_scratch, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  // One decompress for the whole batch, then pure indexing.
  this->decompress(scratch, nullmap, src, tuple_count, level);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = scratch[positions[i]];
  }
}
// -------------------------------------------------------------------------------------
INTEGER IntegerScheme::lookupAt(const u8* src,
                                BitmapWrapper* nullmap,
                                u32 tuple_count,
                                u32 position,
                                u32 level) {
  INTEGER result = 0;
  this->gather(&result, src, nullmap, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
string ConvertSchemeTypeToString(IntegerSchemeType type) {
  switch (type) {
    case IntegerSchemeType::PFOR:
      return "PFOR";
    case IntegerSchemeType::PFOR_DELTA:
      return "PFOR_DETA";
    case IntegerSchemeType::BP:
      return "BP";
    case IntegerSchemeType::RLE:
      return "RLE";
    case IntegerSchemeType::DICT:
      return "DICT";
    case IntegerSchemeType::FREQUENCY:
      return "FREQUENCY";
    case IntegerSchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case IntegerSchemeType::DICTIONARY_8:
      return "DICTIONARY_8";
    case IntegerSchemeType::DICTIONARY_16:
      return "DICTIONARY_16";
    case IntegerSchemeType::TRUNCATION_8:
      return "TRUNCATION_8";
    case IntegerSchemeType::TRUNCATION_16:
      return "TRUNCATION_16";
    case IntegerSchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    case IntegerSchemeType::FOR:
      return "FOR";
    case IntegerSchemeType::SUB_INT_SPLIT:
      return "SUB_INT_SPLIT";
    default:
      throw Generic_Exception("Unknown IntegerSchemeType");
  }
}
// ------------------------------------------------------------------------------
string ConvertSchemeTypeToString(DoubleSchemeType type) {
  switch (type) {
    case DoubleSchemeType::PSEUDODECIMAL:
      return "PSEUDODECIMAL";
    case DoubleSchemeType::RLE:
      return "RLE";
    case DoubleSchemeType::DICT:
      return "DICT";
    case DoubleSchemeType::FREQUENCY:
      return "FREQUENCY";
    case DoubleSchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case DoubleSchemeType::DICTIONARY_8:
      return "DICTIONARY_8";
    case DoubleSchemeType::DICTIONARY_16:
      return "DICTIONARY_16";
    case DoubleSchemeType::DOUBLE_BP:
      return "DOUBLE_BP";
    case DoubleSchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    default:
      throw Generic_Exception("Unknown DoubleSchemeType");
  }
}
// ------------------------------------------------------------------------------
string ConvertSchemeTypeToString(StringSchemeType type) {
  switch (type) {
    case StringSchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case StringSchemeType::DICTIONARY_8:
      return "DICTIONARY_8";
    case StringSchemeType::DICTIONARY_16:
      return "DICTIONARY_16";
    case StringSchemeType::DICT:
      return "S_DICT";
    case StringSchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    case StringSchemeType::FSST:
      return "FSST";
    default:
      throw Generic_Exception("Unknown StringSchemeType");
  }
}
// ------------------------------------------------------------------------------
}  // namespace btrblocks
