#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
struct BP64Structure {
  u32 low_used;  // bytes used by the low-32-bits stream, so the high stream is
                 // addressable without decoding the low one first
  u8 low_scheme_code;
  u8 high_scheme_code;
  u8 data[];  // [low-32-bits stream][high-32-bits stream]
};
// -------------------------------------------------------------------------------------
// The general-purpose 64-bit integer codec: BtrBlocks' vendored FastPFOR
// library has no native 64-bit bit-packing (FastBinaryPacking/SIMDFastPFor's
// encodeArray(const uint64_t*, ...) overloads are unimplemented upstream --
// see extern/FastPFOR.hpp/.cpp and the R1 investigation notes), so this
// splits each value into its low and high 32-bit halves and compresses each
// independently through the *ordinary* 32-bit IntegerSchemePicker -- which
// already auto-selects the best available 32-bit scheme (BP, PFOR, RLE, DICT,
// ...) per half, rather than hardcoding one. Positionally aligned: row i's
// low half and high half are always at row i in their respective streams, so
// no join/lookup is needed to recombine them.
//
// This is what fixes the previous free-standing FBP64 helper's bug (it
// reinterpreted a u64 array as u32 and compressed only tuple_count words,
// silently covering only every other 32-bit half of the input) -- and, as a
// side effect of compressing through the picker rather than a fixed FastPFOR
// call, gather()/lookupAt() come for free by delegating to whichever 32-bit
// scheme each half's picker chose, inheriting all of that scheme's own
// random-access behavior (including FBP's mini-block gather).
class BP64 : public Integer64Scheme {
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
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::BP; }
  BIGINT lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
