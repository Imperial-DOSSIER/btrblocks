// -------------------------------------------------------------------------------------
#include "SubIntSplit64.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/integer/subintsplit/SubIntSplitCore.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
using Core = subintsplit::SubIntSplitCore<u64>;
// -------------------------------------------------------------------------------------
u64 SubIntSplit64::maxCompressedSize(u32 tuple_count) {
  return Core::maxCompressedSize(tuple_count);
}
// -------------------------------------------------------------------------------------
u32 SubIntSplit64::compress(const s64* src,
                            const BITMAP* nullmap,
                            u8* dest,
                            u32 tuple_count,
                            u8 allowed_cascading_level) {
  // Reinterpreted, never converted: only bit patterns matter.
  return Core::encode(reinterpret_cast<const u64*>(src), nullmap, dest, tuple_count,
                      allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void SubIntSplit64::decompress(s64* dest, const u8* src, u32 tuple_count, u32 level) {
  Core::decode(reinterpret_cast<u64*>(dest), src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
void SubIntSplit64::gather(s64* dest,
                           const u8* src,
                           u32 tuple_count,
                           const u32* positions,
                           u32 position_count,
                           u32 level) {
  Core::gather(reinterpret_cast<u64*>(dest), src, tuple_count, positions, position_count, level);
}
// -------------------------------------------------------------------------------------
s64 SubIntSplit64::lookupAt(const u8* src, u32 tuple_count, u32 position, u32 level) {
  s64 result = 0;
  gather(&result, src, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
std::string SubIntSplit64::fullDescription(const u8* src) {
  return Core::describe(src, "SUB_INT_SPLIT_64");
}
// -------------------------------------------------------------------------------------
u8 SubIntSplit64::sectionCount(const u8* src) {
  return Core::sectionCount(src);
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
