#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
class Truncation16 : public IntegerScheme {
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
  inline IntegerSchemeType schemeType() override { return staticSchemeType(); }
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::TRUNCATION_16; }
  // -------------------------------------------------------------------------------------
  INTEGER lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
  virtual bool canCompress(SInteger32Stats& stats) {
    return stats.max - stats.min <= std::numeric_limits<u16>::max();
  }
};
// -------------------------------------------------------------------------------------
class Truncation8 : public IntegerScheme {
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
  inline IntegerSchemeType schemeType() override { return staticSchemeType(); }
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::TRUNCATION_8; }
  // -------------------------------------------------------------------------------------
  INTEGER lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
  virtual bool canCompress(SInteger32Stats& stats) {
    return stats.max - stats.min <= std::numeric_limits<u8>::max();
  }
};
// -------------------------------------------------------------------------------------
// ValueType is explicit (never defaulted) so 32- and 64-bit truncation share
// one implementation: only the value width and its stats type actually
// differ between Truncation8/16 (ValueType = INTEGER, StatsType =
// SInteger32Stats) and Truncation64 (ValueType = BIGINT, StatsType =
// SInteger64Stats). CodeType is the truncated storage width (u8/u16/u32).
template <typename ValueType, typename CodeType>
struct TruncationStructure {
  ValueType base;
  CodeType truncated_values[];
};
// -------------------------------------------------------------------------------------
template <typename CodeType, typename StatsType>
double ITruncExpectedCF(StatsType& stats) {
  if (stats.max - stats.min <= (std::numeric_limits<CodeType>::max())) {
    return sizeof(decltype(stats.max)) / sizeof(CodeType);
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
template <typename CodeType, typename ValueType, typename StatsType>
double ITruncCompress(const ValueType* src, const BITMAP* nullmap, u8* dest, StatsType& stats) {
  die_if(stats.max - stats.min <= std::numeric_limits<CodeType>::max());
  // -------------------------------------------------------------------------------------
  auto& col_struct = *reinterpret_cast<TruncationStructure<ValueType, CodeType>*>(dest);
  // -------------------------------------------------------------------------------------
  // Set the base
  col_struct.base = stats.min;
  // -------------------------------------------------------------------------------------
  // Truncate each integer
  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    if (nullmap == nullptr || nullmap[row_i]) {
      auto biased_value = static_cast<CodeType>(src[row_i] - col_struct.base);
      col_struct.truncated_values[row_i] = biased_value;
    }
  }
  // -------------------------------------------------------------------------------------
  return sizeof(TruncationStructure<ValueType, CodeType>) + (sizeof(CodeType) * stats.tuple_count);
}
// -------------------------------------------------------------------------------------
template <typename CodeType, typename ValueType>
void ITruncDecompress(ValueType* dest,
                      BitmapWrapper* nullmap,
                      const u8* src,
                      u32 tuple_count,
                      u32 level) {
  const auto& col_struct = *reinterpret_cast<const TruncationStructure<ValueType, CodeType>*>(src);
  // ITruncCompress only writes truncated_values[row_i] for non-null rows, so
  // null rows must never be read here -- mirrors that same nullmap branching.
  if (nullmap == nullptr || nullmap->type() == BitmapType::ALLONES) {
    for (u32 row_i = 0; row_i < tuple_count; row_i++) {
      dest[row_i] = col_struct.base + col_struct.truncated_values[row_i];
    }
  } else if (nullmap->type() == BitmapType::ALLZEROS) {
    return;
  } else {
    for (u32 row_i = 0; row_i < tuple_count; row_i++) {
      if (nullmap->test(row_i)) {
        dest[row_i] = col_struct.base + col_struct.truncated_values[row_i];
      }
    }
  }
}
// -------------------------------------------------------------------------------------
// Fixed-width truncated values are directly addressable -- true O(1) random
// access, same reasoning as Uncompressed. Null rows are never requested by a
// well-formed caller (gather/lookupAt operate on already-decoded row
// positions), so unlike decompress() this does not need nullmap branching.
template <typename CodeType, typename ValueType>
void ITruncGather(ValueType* dest,
                  const u8* src,
                  u32 tuple_count,
                  const u32* positions,
                  u32 position_count) {
  if (tuple_count == 0 || position_count == 0) {
    return;
  }
  const auto& col_struct = *reinterpret_cast<const TruncationStructure<ValueType, CodeType>*>(src);
  for (u32 i = 0; i < position_count; i++) {
    dest[i] = col_struct.base + col_struct.truncated_values[positions[i]];
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::integers
