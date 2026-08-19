#include "Truncation64.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme64.hpp"
#include "scheme/integer/Truncation.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
using btrblocks::legacy::integers::ITruncCompress;
using btrblocks::legacy::integers::ITruncDecompress;
using btrblocks::legacy::integers::ITruncExpectedCF;
using btrblocks::legacy::integers::ITruncGather;
// -------------------------------------------------------------------------------------
double Truncation64::expectedCompressionRatio(SInteger64Stats& stats, u8) {
  return ITruncExpectedCF<u32>(stats);
}
// -------------------------------------------------------------------------------------
u32 Truncation64::compress(const BIGINT* src,
                           const BITMAP* nullmap,
                           u8* dest,
                           SInteger64Stats& stats,
                           u8) {
  return ITruncCompress<u32>(src, nullmap, dest, stats);
}
// -------------------------------------------------------------------------------------
void Truncation64::decompress(BIGINT* dest,
                              BitmapWrapper* nullmap,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  ITruncDecompress<u32>(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
void Truncation64::gather(BIGINT* dest,
                          const u8* src,
                          BitmapWrapper*,
                          u32 tuple_count,
                          const u32* positions,
                          u32 position_count,
                          u32) {
  ITruncGather<u32>(dest, src, tuple_count, positions, position_count);
}
BIGINT Truncation64::lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) {
  if (tuple_count == 0) {
    return 0;
  }
  BIGINT result = 0;
  ITruncGather<u32>(&result, src, tuple_count, &position, 1);
  return result;
}
// -------------------------------------------------------------------------------------
BIGINT Truncation64::lookup(u32) {
  UNREACHABLE();
}
void Truncation64::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
