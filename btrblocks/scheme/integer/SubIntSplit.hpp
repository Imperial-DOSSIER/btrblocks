#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
// SubIntSplit decomposes each value into contiguous bit-range sub-streams
// ("sections") whose boundaries are chosen by a sample-driven dynamic program,
// then compresses each section independently through the ordinary scheme
// picker. It targets values built from semantic bit-fields -- snowflake IDs
// (sign | timestamp | machine | sequence), IPv4 addresses, composite keys --
// where different bit ranges have wildly different statistics and no single
// scheme fits the value as a whole.
//
// Every section is at most 32 bits wide, so a section is an ordinary INTEGER
// stream and any registered integer scheme can compress it. See
// docs/subintsplit.md for the wire format and the selection algorithm.
// -------------------------------------------------------------------------------------
class SubIntSplit : public IntegerScheme {
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
  std::string fullDescription(const u8* src) override;
  inline IntegerSchemeType schemeType() override { return staticSchemeType(); }
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::SUB_INT_SPLIT; }
  INTEGER lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
