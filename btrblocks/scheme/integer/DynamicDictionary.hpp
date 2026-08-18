#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
struct DynamicDictionaryStructure {
  u8 codes_scheme_code;
  u32 codes_offset;
  u8 data[];
};
// -------------------------------------------------------------------------------------
class DynamicDictionary : public IntegerScheme {
 public:
  double expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) override;
  u32 compress(const INTEGER* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger32Stats& stats,
               u8 allowed_cascading_level) override;
  void decompress(INTEGER* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  // The dictionary values array is already O(1)-addressable by code, so
  // random access is exactly as good as the codes sub-scheme's own: gather
  // the codes for just the requested positions (batched, one call), then
  // index the dictionary directly -- instead of the base class's full-chunk
  // decode-then-index fallback.
  void gather(INTEGER* dest,
              const u8* src,
              BitmapWrapper* nullmap,
              u32 tuple_count,
              const u32* positions,
              u32 position_count,
              u32 level) override;
  INTEGER lookupAt(const u8* src,
                   BitmapWrapper* nullmap,
                   u32 tuple_count,
                   u32 position,
                   u32 level) override;
  std::string fullDescription(const u8* src) override;
  inline IntegerSchemeType schemeType() override { return staticSchemeType(); }
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::DICT; }
  INTEGER lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
