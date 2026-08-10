// -------------------------------------------------------------------------------------
#include "SubIntSplit.hpp"
// -------------------------------------------------------------------------------------
#include "cache/ThreadCache.hpp"
#include "common/Exceptions.hpp"
#include "common/Units.hpp"
#include "scheme/integer/subintsplit/SubIntSplitCore.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
using Core = subintsplit::SubIntSplitCore<u32>;
// -------------------------------------------------------------------------------------
double SubIntSplit::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  // Splitting an already-split section is meaningless, and would recurse:
  // estimating SubIntSplit runs compress(), which asks the picker to choose a
  // scheme for each section, which estimates SubIntSplit again. Nesting depth
  // is the natural guard, and it covers both the sampling and the try-all
  // selection paths.
  if (ThreadCache::get().compression_level > 1) {
    return 0;
  }
  // Sections need a cascade level of their own. At 1 the picker would force
  // every section to UNCOMPRESSED, which costs a full INTEGER per value per
  // section -- strictly worse than not splitting at all.
  if (allowed_cascading_level < 2) {
    return 0;
  }
  return IntegerScheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 SubIntSplit::compress(const INTEGER* src,
                          const BITMAP* nullmap,
                          u8* dest,
                          SInteger32Stats& stats,
                          u8 allowed_cascading_level) {
  // Reinterpreted, never converted: SubIntSplit works on bit patterns, so it is
  // indifferent to sign. Doing arithmetic on the signed value here is what
  // would break for negative inputs.
  return Core::encode(reinterpret_cast<const u32*>(src), nullmap, dest, stats.tuple_count,
                      allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void SubIntSplit::decompress(INTEGER* dest,
                             BitmapWrapper* nullmap,
                             const u8* src,
                             u32 tuple_count,
                             u32 level) {
  // Null slots round-trip whatever bit pattern they held, as in the dictionary
  // schemes: Chunk equality only compares rows the bitmap marks present.
  (void)nullmap;
  Core::decode(reinterpret_cast<u32*>(dest), src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
std::string SubIntSplit::fullDescription(const u8* src) {
  return Core::describe(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
INTEGER SubIntSplit::lookup(u32) {
  UNREACHABLE();
}
void SubIntSplit::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
