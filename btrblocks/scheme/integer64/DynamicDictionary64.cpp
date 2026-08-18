#include "DynamicDictionary64.hpp"
#include "common/Units.hpp"
#include "common/Utils.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme64.hpp"
#include "scheme/templated/DynamicDictionary.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
using MyDynamicDictionary64 =
    TDynamicDictionary<s64, Integer64Scheme, SInteger64Stats, Integer64SchemeType>;
// -------------------------------------------------------------------------------------
double DynamicDictionary64::expectedCompressionRatio(SInteger64Stats& stats,
                                                     u8 allowed_cascading_level) {
  return MyDynamicDictionary64::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 DynamicDictionary64::compress(const BIGINT* src,
                                  const BITMAP* nullmap,
                                  u8* dest,
                                  SInteger64Stats& stats,
                                  u8 allowed_cascading_level) {
  return MyDynamicDictionary64::compressColumn(src, nullmap, dest, stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void DynamicDictionary64::decompress(BIGINT* dest,
                                     BitmapWrapper* nullmap,
                                     const u8* src,
                                     u32 tuple_count,
                                     u32 level) {
  return MyDynamicDictionary64::decompressColumn(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
namespace {
thread_local std::vector<std::vector<INTEGER>> dict64_codes_scratch;
}  // namespace
// -------------------------------------------------------------------------------------
void DynamicDictionary64::gather(BIGINT* dest,
                                 const u8* src,
                                 BitmapWrapper*,
                                 u32 tuple_count,
                                 const u32* positions,
                                 u32 position_count,
                                 u32 level) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  // Note: this scheme's own (unused, non-packed) DynamicDictionaryStructure
  // declared above must not be used to interpret `src` -- the wire format is
  // laid out by the packed ::btrblocks::DynamicDictionaryStructure that
  // TDynamicDictionary::compressColumn actually writes.
  const auto& col_struct = *reinterpret_cast<const ::btrblocks::DynamicDictionaryStructure*>(src);
  INTEGER* codes = get_level_data(dict64_codes_scratch, position_count, level);
  auto& codes_scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme_code);
  codes_scheme.gather(codes, col_struct.data + col_struct.codes_offset, nullptr, tuple_count,
                      positions, position_count, level + 1);
  const auto* dict = reinterpret_cast<const BIGINT*>(col_struct.data);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = dict[codes[i]];
  }
}
// -------------------------------------------------------------------------------------
BIGINT DynamicDictionary64::lookupAt(const u8* src,
                                     BitmapWrapper* nullmap,
                                     u32 tuple_count,
                                     u32 position,
                                     u32 level) {
  BIGINT result = 0;
  this->gather(&result, src, nullmap, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
BIGINT DynamicDictionary64::lookup(u32) {
  UNREACHABLE();
}
void DynamicDictionary64::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
string DynamicDictionary64::fullDescription(const u8* src) {
  return MyDynamicDictionary64::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
