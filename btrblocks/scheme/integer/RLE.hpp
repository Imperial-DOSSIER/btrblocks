#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
struct RLEStructure {
  u32 runs_count;
  u32 runs_count_offset;
  u8 values_scheme_code;
  u8 counts_scheme_code;
  u8 data[];
};
// -------------------------------------------------------------------------------------
class RLE : public IntegerScheme {
 public:
  double expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) override;
  u32 compress(const INTEGER* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger32Stats& stats,
               u8 allowed_cascading_level) override;
  u32 decompressRuns(INTEGER* values,
                     INTEGER* counts,
                     BitmapWrapper* nullmap,
                     const u8* src,
                     u32 tuple_count,
                     u32 level);
  void decompress(INTEGER* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  // Builds a run-offset index from the (small) counts sub-stream -- O(runs)
  // rather than O(tuple_count) -- binary-searches it per requested position to
  // find the owning run, then gathers those run indices from the values
  // sub-scheme in one batched call. Net cost is O(runs + log(runs) *
  // position_count + values_child_gather_cost), instead of the base class's
  // O(tuple_count) full-column decode.
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
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::RLE; }
  INTEGER lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
