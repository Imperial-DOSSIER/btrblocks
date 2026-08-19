#include "Truncation.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
double Truncation16::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  return ITruncExpectedCF<u16>(stats);
}
// -------------------------------------------------------------------------------------
u32 Truncation16::compress(const INTEGER* src,
                           const BITMAP* nullmap,
                           u8* dest,
                           SInteger32Stats& stats,
                           u8 allowed_cascading_level) {
  return ITruncCompress<u16>(src, nullmap, dest, stats);
}
// -------------------------------------------------------------------------------------
void Truncation16::decompress(INTEGER* dest,
                              BitmapWrapper* nullmap,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  ITruncDecompress<u16>(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
void Truncation16::gather(INTEGER* dest,
                          const u8* src,
                          BitmapWrapper*,
                          u32 tuple_count,
                          const u32* positions,
                          u32 position_count,
                          u32) {
  ITruncGather<u16>(dest, src, tuple_count, positions, position_count);
}
INTEGER Truncation16::lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) {
  if (tuple_count == 0) {
    return 0;
  }
  INTEGER result = 0;
  ITruncGather<u16>(&result, src, tuple_count, &position, 1);
  return result;
}
// -------------------------------------------------------------------------------------
INTEGER Truncation16::lookup(u32) {
  UNREACHABLE();
}
void Truncation16::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
// Truncation with 8 bits
// -------------------------------------------------------------------------------------
double Truncation8::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  return ITruncExpectedCF<u8>(stats);
}
// -------------------------------------------------------------------------------------
u32 Truncation8::compress(const INTEGER* src,
                          const BITMAP* nullmap,
                          u8* dest,
                          SInteger32Stats& stats,
                          u8 allowed_cascading_level) {
  return ITruncCompress<u8>(src, nullmap, dest, stats);
}
// -------------------------------------------------------------------------------------
void Truncation8::decompress(INTEGER* dest,
                             BitmapWrapper* nullmap,
                             const u8* src,
                             u32 tuple_count,
                             u32 level) {
  ITruncDecompress<u8>(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
void Truncation8::gather(INTEGER* dest,
                         const u8* src,
                         BitmapWrapper*,
                         u32 tuple_count,
                         const u32* positions,
                         u32 position_count,
                         u32) {
  ITruncGather<u8>(dest, src, tuple_count, positions, position_count);
}
INTEGER Truncation8::lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) {
  if (tuple_count == 0) {
    return 0;
  }
  INTEGER result = 0;
  ITruncGather<u8>(&result, src, tuple_count, &position, 1);
  return result;
}
// -------------------------------------------------------------------------------------
void Truncation8::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
INTEGER Truncation8::lookup(u32) {
  UNREACHABLE();
}
}  // namespace btrblocks::legacy::integers
// -------------------------------------------------------------------------------------
