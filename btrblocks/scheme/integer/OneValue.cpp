#include "OneValue.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
double OneValue::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  if (stats.distinct_values.size() <= 1) {
    return stats.tuple_count;
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 OneValue::compress(const INTEGER* src, const BITMAP*, u8* dest, SInteger32Stats& stats, u8) {
  auto& col_struct = *reinterpret_cast<OneValueStructure*>(dest);
  if (src != nullptr) {
    col_struct.one_value = stats.distinct_values.begin()->first;
  } else {
    col_struct.one_value = NULL_CODE;
  }
  return sizeof(UINTEGER);
}
// -------------------------------------------------------------------------------------
void OneValue::decompress(INTEGER* dest,
                          BitmapWrapper*,
                          const u8* src,
                          u32 tuple_count,
                          u32 level) {
  const auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {  // can be further optimized probably
    dest[row_i] = col_struct.one_value;
  }
}
// -------------------------------------------------------------------------------------
void OneValue::gather(INTEGER* dest,
                      const u8* src,
                      BitmapWrapper*,
                      u32 tuple_count,
                      const u32*,
                      u32 position_count,
                      u32) {
  // Matches the base class: an empty chunk yields nothing. The two must agree,
  // since the point of the API is that schemes are interchangeable through it.
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  const auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = col_struct.one_value;
  }
}
// -------------------------------------------------------------------------------------
INTEGER OneValue::lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32, u32) {
  if (tuple_count == 0) {
    return 0;
  }
  return reinterpret_cast<const OneValueStructure*>(src)->one_value;
}
// -------------------------------------------------------------------------------------
INTEGER OneValue::lookup(u32) {
  UNREACHABLE();
}
void OneValue::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::integers
// -------------------------------------------------------------------------------------
