#include "FOR64.hpp"
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
double FOR64::expectedCompressionRatio(SInteger64Stats&, u8) {
  return 0;
}
// -------------------------------------------------------------------------------------
u32 FOR64::compress(const BIGINT* src,
                    const BITMAP* nullmap,
                    u8* dest,
                    SInteger64Stats& stats,
                    u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<FOR64Structure*>(dest);
  vector<BIGINT> biased_output;
  // -------------------------------------------------------------------------------------
  col_struct.bias = stats.min;
  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    if (nullmap == nullptr || nullmap[row_i]) {
      biased_output.push_back(src[row_i] - col_struct.bias);
    } else {
      biased_output.push_back(src[row_i]);
    }
  }
  // -------------------------------------------------------------------------------------
  auto write_ptr = col_struct.data;
  u32 used_space;
  Integer64SchemePicker::compress(biased_output.data(), nullmap, write_ptr, biased_output.size(),
                                  allowed_cascading_level - 1, used_space, col_struct.next_scheme,
                                  autoScheme(), "for64_next_level");
  write_ptr += used_space;
  // -------------------------------------------------------------------------------------
  return write_ptr - dest;
}
// -------------------------------------------------------------------------------------
void FOR64::decompress(BIGINT* dest,
                       BitmapWrapper* nullmap,
                       const u8* src,
                       u32 tuple_count,
                       u32 level) {
  const auto& col_struct = *reinterpret_cast<const FOR64Structure*>(src);
  // -------------------------------------------------------------------------------------
  Integer64SchemePicker::MyTypeWrapper::getScheme(col_struct.next_scheme)
      .decompress(dest, nullmap, col_struct.data, tuple_count, level + 1);
  // -------------------------------------------------------------------------------------
  if (nullmap != nullptr && nullmap->type() == BitmapType::ALLZEROS) {
    return;
  }
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] += col_struct.bias;
  }
}
// -------------------------------------------------------------------------------------
void FOR64::gather(BIGINT* dest,
                   const u8* src,
                   BitmapWrapper* nullmap,
                   u32 tuple_count,
                   const u32* positions,
                   u32 position_count,
                   u32 level) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  const auto& col_struct = *reinterpret_cast<const FOR64Structure*>(src);
  Integer64SchemePicker::MyTypeWrapper::getScheme(col_struct.next_scheme)
      .gather(dest, col_struct.data, nullmap, tuple_count, positions, position_count, level + 1);
  if (nullmap != nullptr && nullmap->type() == BitmapType::ALLZEROS) {
    return;
  }
  for (u32 i = 0; i < position_count; i++) {
    dest[i] += col_struct.bias;
  }
}
// -------------------------------------------------------------------------------------
BIGINT FOR64::lookupAt(const u8* src, BitmapWrapper* nullmap, u32 tuple_count, u32 position, u32 level) {
  BIGINT result = 0;
  this->gather(&result, src, nullmap, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
BIGINT FOR64::lookup(u32) {
  UNREACHABLE();
}
void FOR64::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
std::string FOR64::fullDescription(const u8* src) {
  const auto& col_struct = *reinterpret_cast<const FOR64Structure*>(src);
  auto& scheme = Integer64SchemePicker::MyTypeWrapper::getScheme(col_struct.next_scheme);
  return this->selfDescription() + " -> ([bigint] biased) " + scheme.fullDescription(col_struct.data);
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
