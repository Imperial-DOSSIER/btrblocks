#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme64.hpp"
#include "scheme/templated/FixedDictionary.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers64 {
// -------------------------------------------------------------------------------------
// Native 64-bit siblings of legacy::integers::Dictionary8/Dictionary16,
// sharing FixedDictionaryStructure<BIGINT>/FDict*<CodeType, BIGINT, ...>
// (scheme/templated/FixedDictionary.hpp). Both the dictionary and the codes
// are plain fixed-width arrays -- true O(1) random access, no recursion
// needed (unlike DynamicDictionary64, whose codes are themselves
// compressed). Opt-in only, matching the 32-bit Dictionary8/16's legacy,
// not-in-defaultIntegerSchemes() status.
class Dictionary16_64 : public Integer64Scheme {
 public:
  double expectedCompressionRatio(SInteger64Stats& stats, u8 allowed_cascading_level) override {
    return FDictExpectedCompressionRatio<u16, BIGINT>(stats);
  }
  u32 compress(const BIGINT* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger64Stats& stats,
               u8) override {
    return FDictCompressColumn<u16, BIGINT>(src, nullmap, dest, stats);
  }
  void decompress(BIGINT* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override {
    return FDictDecompressColumn<u16, BIGINT>(dest, nullmap, src, tuple_count, level);
  }
  void gather(BIGINT* dest,
              const u8* src,
              BitmapWrapper*,
              u32 tuple_count,
              const u32* positions,
              u32 position_count,
              u32) override {
    if (tuple_count == 0 || position_count == 0) {
      return;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<BIGINT>*>(src);
    const auto* codes = reinterpret_cast<const u16*>(src + col_struct.codes_offset);
    for (u32 i = 0; i < position_count; i++) {
      dest[i] = col_struct.dict_slots[codes[positions[i]]];
    }
  }
  BIGINT lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) override {
    if (tuple_count == 0) {
      return 0;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<BIGINT>*>(src);
    const auto* codes = reinterpret_cast<const u16*>(src + col_struct.codes_offset);
    return col_struct.dict_slots[codes[position]];
  }
  inline Integer64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::DICTIONARY_16; }
  BIGINT lookup(u32) override { UNREACHABLE(); }
  void scan(Predicate, BITMAP*, const u8*, u32) override { UNREACHABLE(); }
};
// -------------------------------------------------------------------------------------
class Dictionary8_64 : public Integer64Scheme {
 public:
  double expectedCompressionRatio(SInteger64Stats& stats, u8 allowed_cascading_level) override {
    return FDictExpectedCompressionRatio<u8, BIGINT>(stats);
  }
  u32 compress(const BIGINT* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger64Stats& stats,
               u8) override {
    return FDictCompressColumn<u8, BIGINT>(src, nullmap, dest, stats);
  }
  void decompress(BIGINT* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override {
    return FDictDecompressColumn<u8, BIGINT>(dest, nullmap, src, tuple_count, level);
  }
  // See Dictionary16_64::gather -- same reasoning, u8 codes here.
  void gather(BIGINT* dest,
              const u8* src,
              BitmapWrapper*,
              u32 tuple_count,
              const u32* positions,
              u32 position_count,
              u32) override {
    if (tuple_count == 0 || position_count == 0) {
      return;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<BIGINT>*>(src);
    const auto* codes = src + col_struct.codes_offset;
    for (u32 i = 0; i < position_count; i++) {
      dest[i] = col_struct.dict_slots[codes[positions[i]]];
    }
  }
  BIGINT lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) override {
    if (tuple_count == 0) {
      return 0;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<BIGINT>*>(src);
    const auto* codes = src + col_struct.codes_offset;
    return col_struct.dict_slots[codes[position]];
  }
  inline Integer64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Integer64SchemeType staticSchemeType() { return Integer64SchemeType::DICTIONARY_8; }
  BIGINT lookup(u32) override { UNREACHABLE(); }
  void scan(Predicate, BITMAP*, const u8*, u32) override { UNREACHABLE(); }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers64
