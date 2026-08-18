#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme64.hpp"
#include "scheme/integer/Truncation.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
// Native 64-bit truncation, sharing the (now width-generic) ITrunc* function
// templates in scheme/integer/Truncation.hpp with the 32-bit Truncation8/16 --
// see that header's TruncationStructure<ValueType, CodeType> comment. Unlike
// the 32-bit side (which offers u8 and u16 code widths), BIGINT columns get a
// single u32-coded variant: a 32-bit code already halves an 8-byte value, and
// a column whose range fits in fewer bits than that will simply be beaten to
// the punch by FOR64/BP64 during scheme selection, so splitting further into
// Truncation64_8/16 buys little. Fixed-width truncated values are directly
// addressable -- true O(1) random access, same reasoning as Uncompressed64.
class Truncation64 : public Integer64Scheme {
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
  inline Integer64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::TRUNCATION; }
  BIGINT lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
  bool canCompress(SInteger64Stats& stats) {
    return static_cast<UBIGINT>(stats.max - stats.min) <=
           static_cast<UBIGINT>(std::numeric_limits<u32>::max());
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
