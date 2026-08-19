#include "Uncompressed.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
double Uncompressed::expectedCompressionRatio(SInteger32Stats&, u8 allowed_cascading_level) {
  return 1.0;
}
// -------------------------------------------------------------------------------------
u32 Uncompressed::compress(const INTEGER* src,
                           const BITMAP*,
                           u8* dest,
                           SInteger32Stats& stats,
                           u8 allowed_cascading_level) {
  const u32 column_size = stats.total_size;
  std::memcpy(dest, src, column_size);
  return column_size;
}
// -------------------------------------------------------------------------------------
void Uncompressed::decompress(INTEGER* dest,
                              BitmapWrapper*,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  const u32 column_size = tuple_count * sizeof(UINTEGER);
  std::memcpy(dest, src, column_size);
}
// -------------------------------------------------------------------------------------
void Uncompressed::gather(INTEGER* dest,
                          const u8* src,
                          BitmapWrapper*,
                          u32 tuple_count,
                          const u32* positions,
                          u32 position_count,
                          u32) {
  // Matches the base class: an empty chunk yields nothing rather than reading
  // past the buffer. The two must agree, since the point of the API is that
  // schemes are interchangeable through it.
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  const auto* values = reinterpret_cast<const INTEGER*>(src);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = values[positions[i]];
  }
}
// -------------------------------------------------------------------------------------
INTEGER Uncompressed::lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) {
  if (tuple_count == 0) {
    return 0;
  }
  return reinterpret_cast<const INTEGER*>(src)[position];
}
// -------------------------------------------------------------------------------------
INTEGER Uncompressed::lookup(u32) {
  UNREACHABLE();
}
void Uncompressed::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
}  // namespace btrblocks::legacy::integers
// -------------------------------------------------------------------------------------
