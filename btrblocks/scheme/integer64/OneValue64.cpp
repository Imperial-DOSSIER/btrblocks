#include "OneValue64.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
double OneValue64::expectedCompressionRatio(SInteger64Stats& stats, u8) {
  if (stats.distinct_values.size() <= 1) {
    return stats.tuple_count;
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 OneValue64::compress(const BIGINT* src, const BITMAP*, u8* dest, SInteger64Stats& stats, u8) {
  auto& col_struct = *reinterpret_cast<OneValue64Structure*>(dest);
  if (src != nullptr) {
    col_struct.one_value = stats.distinct_values.begin()->first;
  } else {
    col_struct.one_value = NULL_CODE;
  }
  return sizeof(UBIGINT);
}
// -------------------------------------------------------------------------------------
void OneValue64::decompress(BIGINT* dest,
                            BitmapWrapper*,
                            const u8* src,
                            u32 tuple_count,
                            u32) {
  const auto& col_struct = *reinterpret_cast<const OneValue64Structure*>(src);
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] = col_struct.one_value;
  }
}
// -------------------------------------------------------------------------------------
void OneValue64::gather(BIGINT* dest,
                        const u8* src,
                        BitmapWrapper*,
                        u32 tuple_count,
                        const u32*,
                        u32 position_count,
                        u32) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  const auto& col_struct = *reinterpret_cast<const OneValue64Structure*>(src);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = col_struct.one_value;
  }
}
// -------------------------------------------------------------------------------------
BIGINT OneValue64::lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32, u32) {
  if (tuple_count == 0) {
    return 0;
  }
  return reinterpret_cast<const OneValue64Structure*>(src)->one_value;
}
// -------------------------------------------------------------------------------------
BIGINT OneValue64::lookup(u32) {
  UNREACHABLE();
}
void OneValue64::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
