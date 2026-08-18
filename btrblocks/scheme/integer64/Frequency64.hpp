#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
#include <roaring/roaring.hh>
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
struct FrequencyStructure {
  UBIGINT top_value;
  u32 exceptions_offset;
  u8 next_scheme;
  u8 data[];
};
// -------------------------------------------------------------------------------------
// Native 64-bit sibling of integers::Frequency, sharing TFrequency<...>
// (scheme/templated/Frequency.hpp), which is already fully generic (a
// roaring bitmap of exception positions, no type-specific specializations at
// all). Most positions hold the dominant value directly (O(1) roaring
// membership check); the rest are looked up by roaring rank -> exception
// index, then gathered from the exceptions sub-scheme in one batched call.
// Exceptions are compressed through the *same-width* picker
// (Integer64SchemePicker), matching TFrequency::compressColumn.
class Frequency64 : public Integer64Scheme {
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
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::FREQUENCY; }
  BIGINT lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
