#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
struct OneValue64Structure {
  UBIGINT one_value;
};
// -------------------------------------------------------------------------------------
class OneValue64 : public Integer64Scheme {
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
  // Every row holds the same value, so a row can be read without materializing
  // the chunk. Mirrors the 32-bit OneValue scheme.
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
  inline Integer64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::ONE_VALUE; }
  BIGINT lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
