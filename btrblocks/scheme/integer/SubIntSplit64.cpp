// -------------------------------------------------------------------------------------
#include "SubIntSplit64.hpp"
// -------------------------------------------------------------------------------------
#include "cache/ThreadCache.hpp"
#include "common/Exceptions.hpp"
#include "common/Units.hpp"
#include "scheme/integer/subintsplit/SubIntSplitCore.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
using Core = subintsplit::SubIntSplitCore<u64>;
// -------------------------------------------------------------------------------------
double SubIntSplit64::expectedCompressionRatio(SInteger64Stats& stats, u8 allowed_cascading_level) {
  // Same guard as the 32-bit SubIntSplit: splitting an already-split section
  // is meaningless and would recurse (estimating runs compress(), which asks
  // the picker to choose a scheme for each section, which estimates
  // SubIntSplit again).
  if (ThreadCache::get().compression_level > 1) {
    return 0;
  }
  // Sections need a cascade level of their own. At 1 the picker would force
  // every section to UNCOMPRESSED, which costs a full INTEGER per value per
  // section -- strictly worse than not splitting at all.
  if (allowed_cascading_level < 2) {
    return 0;
  }
  return Integer64Scheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 SubIntSplit64::compress(const BIGINT* src,
                            const BITMAP* nullmap,
                            u8* dest,
                            SInteger64Stats& stats,
                            u8 allowed_cascading_level) {
  // Reinterpreted, never converted: only bit patterns matter.
  return Core::encode(reinterpret_cast<const u64*>(src), nullmap, dest, stats.tuple_count,
                      allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void SubIntSplit64::decompress(BIGINT* dest,
                               BitmapWrapper* nullmap,
                               const u8* src,
                               u32 tuple_count,
                               u32 level) {
  // Null slots round-trip whatever bit pattern they held, as in the
  // dictionary schemes: Chunk equality only compares rows the bitmap marks
  // present.
  (void)nullmap;
  Core::decode(reinterpret_cast<u64*>(dest), src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
void SubIntSplit64::gather(BIGINT* dest,
                           const u8* src,
                           BitmapWrapper* nullmap,
                           u32 tuple_count,
                           const u32* positions,
                           u32 position_count,
                           u32 level) {
  (void)nullmap;
  Core::gather(reinterpret_cast<u64*>(dest), src, tuple_count, positions, position_count, level);
}
// -------------------------------------------------------------------------------------
BIGINT SubIntSplit64::lookupAt(const u8* src, BitmapWrapper* nullmap, u32 tuple_count, u32 position, u32 level) {
  BIGINT result = 0;
  this->gather(&result, src, nullmap, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
std::string SubIntSplit64::fullDescription(const u8* src) {
  return Core::describe(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
BIGINT SubIntSplit64::lookup(u32) {
  UNREACHABLE();
}
void SubIntSplit64::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
u64 SubIntSplit64::maxCompressedSize(u32 tuple_count) {
  return Core::maxCompressedSize(tuple_count);
}
// -------------------------------------------------------------------------------------
u8 SubIntSplit64::sectionCount(const u8* src) {
  return Core::sectionCount(src);
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
