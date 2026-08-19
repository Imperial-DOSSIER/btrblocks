#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/SchemeType.hpp"
// -------------------------------------------------------------------------------------
#include "extern/RoaringBitmap.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
string ConvertSchemeTypeToString(Integer64SchemeType type);
// -------------------------------------------------------------------------------------
// Native 64-bit sibling of IntegerScheme, for ColumnType::BIGINT columns.
// Deliberately a separate base class rather than a templated IntegerScheme<T>:
// the existing 32-bit hierarchy, picker specialization and every 32-bit codec
// stay untouched, so this is purely additive and cannot regress the
// already-measured 32-bit path. See docs/subintsplit.md and the plan this was
// built from for the rationale.
//
// Mirrors IntegerScheme's shape exactly (compress/decompress/gather/lookupAt/
// schemeType/isUsable) with s64/SInteger64Stats/Integer64SchemeType in place
// of INTEGER/SInteger32Stats/IntegerSchemeType.
// -------------------------------------------------------------------------------------
class Integer64Scheme {
 public:
  // -------------------------------------------------------------------------------------
  virtual double expectedCompressionRatio(SInteger64Stats& stats,
                                          [[maybe_unused]] u8 allowed_cascading_level);
  // -------------------------------------------------------------------------------------
  virtual u32 compress(const BIGINT* src,
                       const BITMAP* nullmap,
                       u8* dest,
                       SInteger64Stats& stats,
                       u8 allowed_cascading_level) = 0;
  // -------------------------------------------------------------------------------------
  virtual void decompress(BIGINT* dest,
                          BitmapWrapper* nullmap,
                          const u8* src,
                          u32 tuple_count,
                          u32 level) = 0;
  // -------------------------------------------------------------------------------------
  // Random access within a single chunk. Same contract as IntegerScheme's
  // gather/lookupAt (see CompressionScheme.hpp): the default implementation
  // decompresses the chunk once into thread-local scratch and indexes it --
  // an honest O(tuple_count + position_count) baseline that every *64 codec
  // gets from day one. Schemes able to address a single row without
  // materializing the chunk override these, mirroring the 32-bit codecs'
  // random-access work one-for-one.
  // -------------------------------------------------------------------------------------
  virtual void gather(BIGINT* dest,
                      const u8* src,
                      BitmapWrapper* nullmap,
                      u32 tuple_count,
                      const u32* positions,
                      u32 position_count,
                      u32 level);
  // -------------------------------------------------------------------------------------
  virtual BIGINT lookupAt(const u8* src,
                          BitmapWrapper* nullmap,
                          u32 tuple_count,
                          u32 position,
                          u32 level);
  // -------------------------------------------------------------------------------------
  virtual Integer64SchemeType schemeType() = 0;
  // -------------------------------------------------------------------------------------
  virtual BIGINT lookup(u32 id) = 0;
  // -------------------------------------------------------------------------------------
  virtual void scan(Predicate, BITMAP* result, const u8* src, u32 tuple_count) = 0;
  // -------------------------------------------------------------------------------------
  inline string selfDescription() { return ConvertSchemeTypeToString(this->schemeType()); }
  virtual string fullDescription(const u8*) {
    // Default implementation for schemes that do not have nested schemes
    return this->selfDescription();
  }
  virtual bool isUsable(SInteger64Stats&) { return true; }
  // -------------------------------------------------------------------------------------
  virtual ~Integer64Scheme() = default;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
