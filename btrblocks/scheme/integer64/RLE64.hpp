#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
struct RLEStructure {
  u32 runs_count;
  u32 runs_count_offset;
  u8 values_scheme_code;
  u8 counts_scheme_code;
  u8 data[];
};
// -------------------------------------------------------------------------------------
// Native 64-bit sibling of integers::RLE, sharing TRLE<...> (see
// scheme/templated/RLE.hpp) parameterized on s64/Integer64Scheme/
// SInteger64Stats/Integer64SchemeType instead of INTEGER/IntegerScheme/
// SInteger32Stats/IntegerSchemeType. Run lengths always fit comfortably in 32
// bits even for enormous chunks, so the counts sub-stream is compressed
// through the ordinary 32-bit IntegerSchemePicker regardless of value width --
// same choice TRLE::compressColumn makes for the 32-bit column too.
class RLE64 : public Integer64Scheme {
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
  // Builds a run-offset index from the (small) counts sub-stream -- O(runs)
  // rather than O(tuple_count) -- binary-searches it per requested position,
  // then gathers those run indices from the values sub-scheme in one batched
  // call. Mirrors integers::RLE::gather exactly.
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
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::RLE; }
  BIGINT lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
// -------------------------------------------------------------------------------------
