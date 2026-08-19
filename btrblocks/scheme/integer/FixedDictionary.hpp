#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
#include "scheme/templated/FixedDictionary.hpp"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
class Dictionary16 : public IntegerScheme {
 public:
  double expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) override {
    return FDictExpectedCompressionRatio<u16, INTEGER>(stats);
  }
  // -------------------------------------------------------------------------------------
  u32 compress(const INTEGER* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger32Stats& stats,
               u8) override {
    return FDictCompressColumn<u16, INTEGER>(src, nullmap, dest, stats);
  }
  void decompress(INTEGER* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override {
    return FDictDecompressColumn<u16>(dest, nullmap, src, tuple_count, level);
  }
  // -------------------------------------------------------------------------------------
  // Both the dictionary and the codes are plain fixed-width arrays -- true
  // O(1) random access, no recursion needed (unlike DynamicDictionary, whose
  // codes are themselves compressed).
  void gather(INTEGER* dest,
              const u8* src,
              BitmapWrapper*,
              u32 tuple_count,
              const u32* positions,
              u32 position_count,
              u32) override {
    if (tuple_count == 0 || position_count == 0) {
      return;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<INTEGER>*>(src);
    const auto* codes = reinterpret_cast<const u16*>(src + col_struct.codes_offset);
    for (u32 i = 0; i < position_count; i++) {
      dest[i] = col_struct.dict_slots[codes[positions[i]]];
    }
  }
  INTEGER lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) override {
    if (tuple_count == 0) {
      return 0;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<INTEGER>*>(src);
    const auto* codes = reinterpret_cast<const u16*>(src + col_struct.codes_offset);
    return col_struct.dict_slots[codes[position]];
  }
  // -------------------------------------------------------------------------------------
  inline IntegerSchemeType schemeType() override { return staticSchemeType(); }
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::DICTIONARY_16; }
  // -------------------------------------------------------------------------------------
  INTEGER lookup(u32) override { UNREACHABLE(); }
  void scan(Predicate, BITMAP*, const u8*, u32) override { UNREACHABLE(); }
};
// -------------------------------------------------------------------------------------
class Dictionary8 : public IntegerScheme {
 public:
  double expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) override {
    return FDictExpectedCompressionRatio<u8, UINTEGER>(stats);
  }
  // -------------------------------------------------------------------------------------
  u32 compress(const INTEGER* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger32Stats& stats,
               u8 allowed_cascading_level) override {
    return FDictCompressColumn<u8, INTEGER>(src, nullmap, dest, stats);
  }
  void decompress(INTEGER* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override {
    return FDictDecompressColumn<u8, INTEGER>(dest, nullmap, src, tuple_count, level);
  }
  // -------------------------------------------------------------------------------------
  // See Dictionary16::gather -- same reasoning, u8 codes here.
  void gather(INTEGER* dest,
              const u8* src,
              BitmapWrapper*,
              u32 tuple_count,
              const u32* positions,
              u32 position_count,
              u32) override {
    if (tuple_count == 0 || position_count == 0) {
      return;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<INTEGER>*>(src);
    const auto* codes = src + col_struct.codes_offset;
    for (u32 i = 0; i < position_count; i++) {
      dest[i] = col_struct.dict_slots[codes[positions[i]]];
    }
  }
  INTEGER lookupAt(const u8* src, BitmapWrapper*, u32 tuple_count, u32 position, u32) override {
    if (tuple_count == 0) {
      return 0;
    }
    const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<INTEGER>*>(src);
    const auto* codes = src + col_struct.codes_offset;
    return col_struct.dict_slots[codes[position]];
  }
  // -------------------------------------------------------------------------------------
  inline IntegerSchemeType schemeType() override { return staticSchemeType(); }
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::DICTIONARY_8; }
  // -------------------------------------------------------------------------------------
  INTEGER lookup(u32) override { UNREACHABLE(); }
  void scan(Predicate, BITMAP*, const u8*, u32) override { UNREACHABLE(); }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::integers
