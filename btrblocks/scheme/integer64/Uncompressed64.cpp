#include "Uncompressed64.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
double Uncompressed64::expectedCompressionRatio(SInteger64Stats&, u8) {
  return 1.0;
}
// -------------------------------------------------------------------------------------
u32 Uncompressed64::compress(const BIGINT* src, const BITMAP*, u8* dest, SInteger64Stats& stats, u8) {
  const u32 column_size = stats.total_size;
  std::memcpy(dest, src, column_size);
  return column_size;
}
// -------------------------------------------------------------------------------------
void Uncompressed64::decompress(BIGINT* dest,
                                BitmapWrapper*,
                                const u8* src,
                                u32 tuple_count,
                                u32) {
  const u32 column_size = tuple_count * sizeof(UBIGINT);
  std::memcpy(dest, src, column_size);
}
// -------------------------------------------------------------------------------------
void Uncompressed64::gather(BIGINT* dest,
                            const u8* src,
                            BitmapWrapper*,
                            u32 tuple_count,
                            const u32* positions,
                            u32 position_count,
                            u32) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  const auto* values = reinterpret_cast<const BIGINT*>(src);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = values[positions[i]];
  }
}
// -------------------------------------------------------------------------------------
BIGINT Uncompressed64::lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) {
  if (tuple_count == 0) {
    return 0;
  }
  return reinterpret_cast<const BIGINT*>(src)[position];
}
// -------------------------------------------------------------------------------------
BIGINT Uncompressed64::lookup(u32) {
  UNREACHABLE();
}
void Uncompressed64::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
