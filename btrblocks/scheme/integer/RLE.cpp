#include "RLE.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/SchemePool.hpp"
#include "scheme/templated/RLE.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
using MyRLE = TRLE<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>;
// -------------------------------------------------------------------------------------
double RLE::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  if (stats.average_run_length < SchemeConfig::get().integers.rle_run_length_threshold) {
    return 0;
  }
  return IntegerScheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 RLE::compress(const INTEGER* src,
                  const BITMAP* nullmap,
                  u8* dest,
                  SInteger32Stats& stats,
                  u8 allowed_cascading_level) {
  auto& cfg = SchemeConfig::get().integers;
  return MyRLE::compressColumn(src, nullmap, dest, stats, allowed_cascading_level,
                               CB(cfg.rle_force_values_scheme), CB(cfg.rle_force_counts_scheme));
}
// -------------------------------------------------------------------------------------
void RLE::decompress(INTEGER* dest,
                     BitmapWrapper* nullmap,
                     const u8* src,
                     u32 tuple_count,
                     u32 level) {
  return MyRLE::decompressColumn(dest, nullmap, src, tuple_count, level);
}
u32 RLE::decompressRuns(INTEGER* values,
                        INTEGER* counts,
                        BitmapWrapper* nullmap,
                        const u8* src,
                        u32 tuple_count,
                        u32 level) {
  return MyRLE::decompressRuns(values, counts, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
namespace {
// Own scratch stacks, indexed by cascade level like the other schemes' -- see
// CompressionScheme.cpp's comment on random_access_scratch for why.
thread_local std::vector<std::vector<INTEGER>> rle_counts_scratch;
thread_local std::vector<std::vector<u32>> rle_offsets_scratch;
}  // namespace
// -------------------------------------------------------------------------------------
void RLE::gather(INTEGER* dest,
                 const u8* src,
                 BitmapWrapper*,
                 u32 tuple_count,
                 const u32* positions,
                 u32 position_count,
                 u32 level) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  const auto& col_struct = *reinterpret_cast<const RLEStructure*>(src);
  const u32 runs_count = col_struct.runs_count;
  // -------------------------------------------------------------------------------------
  // Decode only the counts sub-stream (O(runs_count)), not the whole column.
  INTEGER* counts =
      get_level_data(rle_counts_scratch, runs_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  {
    auto& counts_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.counts_scheme_code);
    counts_scheme.decompress(counts, nullptr, col_struct.data + col_struct.runs_count_offset,
                             runs_count, level + 1);
  }
  // -------------------------------------------------------------------------------------
  // Prefix-sum into a run-offset index: offsets[r] is the first row of run r.
  u32* offsets = get_level_data(rle_offsets_scratch, runs_count + 1, level);
  offsets[0] = 0;
  for (u32 run_i = 0; run_i < runs_count; run_i++) {
    offsets[run_i + 1] = offsets[run_i] + static_cast<u32>(counts[run_i]);
  }
  // -------------------------------------------------------------------------------------
  // Binary-search each position to its owning run: O(log runs_count) each.
  std::vector<u32> run_indices(position_count);
  for (u32 i = 0; i < position_count; i++) {
    u32 lo = 0;
    u32 hi = runs_count;
    while (lo < hi) {
      const u32 mid = lo + (hi - lo) / 2;
      if (offsets[mid + 1] <= positions[i]) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    run_indices[i] = lo;
  }
  // -------------------------------------------------------------------------------------
  // One batched gather into the values sub-scheme -- as good as its own
  // random access, not a full decode of it either.
  auto& values_scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.values_scheme_code);
  values_scheme.gather(dest, col_struct.data, nullptr, runs_count, run_indices.data(),
                       position_count, level + 1);
}
// -------------------------------------------------------------------------------------
INTEGER RLE::lookupAt(const u8* src, BitmapWrapper* nullmap, u32 tuple_count, u32 position, u32 level) {
  INTEGER result = 0;
  this->gather(&result, src, nullmap, tuple_count, &position, 1, level);
  return result;
}
// -------------------------------------------------------------------------------------
INTEGER RLE::lookup(u32) {
  UNREACHABLE();
}
void RLE::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}

std::string RLE::fullDescription(const u8* src) {
  return MyRLE::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
