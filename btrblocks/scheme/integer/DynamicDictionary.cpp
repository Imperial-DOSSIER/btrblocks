#include "DynamicDictionary.hpp"
#include "common/Units.hpp"
#include "common/Utils.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/templated/DynamicDictionary.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"

// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
using MyDynamicDictionary =
    TDynamicDictionary<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>;
// -------------------------------------------------------------------------------------
double DynamicDictionary::expectedCompressionRatio(SInteger32Stats& stats,
                                                   u8 allowed_cascading_level) {
  return MyDynamicDictionary::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 DynamicDictionary::compress(const INTEGER* src,
                                const BITMAP* nullmap,
                                u8* dest,
                                SInteger32Stats& stats,
                                u8 allowed_cascading_level) {
  return MyDynamicDictionary::compressColumn(src, nullmap, dest, stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void DynamicDictionary::decompress(INTEGER* dest,
                                   BitmapWrapper* nullmap,
                                   const u8* src,
                                   u32 tuple_count,
                                   u32 level) {
  return MyDynamicDictionary::decompressColumn(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace {
// Own scratch stack, indexed by cascade level like the other schemes' -- see
// CompressionScheme.cpp's comment on random_access_scratch for why.
thread_local std::vector<std::vector<INTEGER>> dict_codes_scratch;
}  // namespace
// -------------------------------------------------------------------------------------
void DynamicDictionary::gather(INTEGER* dest,
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
  INTEGER* codes = get_level_data(dict_codes_scratch, position_count, level);
  auto& codes_scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme_code);
  codes_scheme.gather(codes, col_struct.data + col_struct.codes_offset, nullptr, tuple_count,
                      positions, position_count, level + 1);
  const auto* dict = reinterpret_cast<const INTEGER*>(col_struct.data);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = dict[codes[i]];
  }
}
// -------------------------------------------------------------------------------------
INTEGER DynamicDictionary::lookupAt(const u8* src,
                                    BitmapWrapper* nullmap,
                                    u32 tuple_count,
                                    u32 position,
                                    u32 level) {
  INTEGER result = 0;
  this->gather(&result, src, nullmap, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
INTEGER DynamicDictionary::lookup(u32) {
  UNREACHABLE();
}
void DynamicDictionary::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
string DynamicDictionary::fullDescription(const u8* src) {
  return MyDynamicDictionary::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
