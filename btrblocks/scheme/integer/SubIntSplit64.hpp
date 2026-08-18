#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <string>
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
// SubIntSplit for 64-bit columns.
//
// TODO(64-7): promote this to a registered Integer64Scheme subclass now that
// Integer64Scheme/Integer64SchemeType exist (see scheme/CompressionScheme64.hpp)
// -- it is currently still free-standing and driven directly rather than
// selected by the picker; this comment predates that scaffolding.
//
// That costs less than it sounds. Sections are capped at 32 bits, so once a
// value is decomposed each section is an ordinary INTEGER stream and the whole
// existing scheme pool compresses it. Only the extraction and accumulation
// loops are 64-bit; everything between them is the 32-bit machinery unchanged.
//
// Bit patterns are round-tripped, so signedness is irrelevant: values are taken
// as u64 and never arithmetically manipulated.
//
// The value is not split into two 32-bit halves. Where the boundaries fall is
// the entire question the planner answers, and 0-31/32-63 is just one candidate
// answer among many -- one the benchmark uses as a control arm.
class SubIntSplit64 {
 public:
  // Bytes an encoding of `tuple_count` values can occupy at most, so callers
  // can size a destination buffer. Encoding never exceeds this.
  static u64 maxCompressedSize(u32 tuple_count);

  // Returns the number of bytes written to `dest`.
  static u32 compress(const s64* src,
                      const BITMAP* nullmap,
                      u8* dest,
                      u32 tuple_count,
                      u8 allowed_cascading_level);

  static void decompress(s64* dest, const u8* src, u32 tuple_count, u32 level);

  // Random access. `positions` are row indices in [0, tuple_count).
  //
  // Sections are decoded whole, because BtrBlocks sub-schemes have no range or
  // offset decode, so this saves the accumulation work for unwanted rows but
  // not the sub-scheme work. See docs/subintsplit.md.
  static void gather(s64* dest,
                     const u8* src,
                     u32 tuple_count,
                     const u32* positions,
                     u32 position_count,
                     u32 level);

  static s64 lookupAt(const u8* src, u32 tuple_count, u32 position, u32 level);

  // The split and the scheme chosen for each section.
  static std::string fullDescription(const u8* src);

  static u8 sectionCount(const u8* src);
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
