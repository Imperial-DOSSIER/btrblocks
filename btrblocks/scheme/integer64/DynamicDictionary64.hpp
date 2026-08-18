#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
struct DynamicDictionaryStructure {
  u8 codes_scheme_code;
  u32 codes_offset;
  u8 data[];
};
// -------------------------------------------------------------------------------------
// Native 64-bit sibling of integers::DynamicDictionary, sharing
// TDynamicDictionary<...> (scheme/templated/DynamicDictionary.hpp)
// parameterized on s64/Integer64Scheme/SInteger64Stats/Integer64SchemeType.
// The dictionary values array is already O(1)-addressable by code, so
// random access is exactly as good as the codes sub-scheme's own: gather the
// codes for just the requested positions (batched, one call), then index the
// dictionary directly.
class DynamicDictionary64 : public Integer64Scheme {
 public:
  double expectedCompressionRatio(SInteger64Stats& stats, u8 allowed_cascading_level) override;
  u32 compress(const BIGINT* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger64Stats& stats,
               u8 allowed_cascading_level) override;
  void decompress(BIGINT* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  void gather(BIGINT* dest,
              const u8* src,
              BitmapWrapper* nullmap,
              u32 tuple_count,
              const u32* positions,
              u32 position_count,
              u32 level) override;
  BIGINT lookupAt(const u8* src,
                  BitmapWrapper* nullmap,
                  u32 tuple_count,
                  u32 position,
                  u32 level) override;
  std::string fullDescription(const u8* src) override;
  inline Integer64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::DICT; }
  BIGINT lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
