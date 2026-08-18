#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <memory>
#include <type_traits>
// -------------------------------------------------------------------------------------
// the linker breaks when including the fastpfor library multiple times.
// thus, provide a wrapper for the functions we use from it.
// -------------------------------------------------------------------------------------
enum class FastPForCodec { FPF, FBP };
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
struct LemiereImpl {
  using u32 = btrblocks::u32;
  using data_t = btrblocks::u32;
  // -------------------------------------------------------------------------------------
  LemiereImpl();
  ~LemiereImpl();
  // -------------------------------------------------------------------------------------
  u32 compress(const data_t* src, u32 count, data_t* dest, size_t& outsize);
  const data_t* decompress(const data_t* src, u32 count, data_t* dest, size_t& outsize);
  // -------------------------------------------------------------------------------------
  // Block-level random access into the block-packed (FastBinaryPacking)
  // region of a compressed stream -- only meaningful for FastPForCodec::FBP;
  // both throw for FPF. SIMDFastPFor (PFOR) groups values into much larger
  // (64K-value) pages with a patched-exception layout that isn't cheaply
  // seekable the way fixed 128-value BinaryPacking blocks are, so true
  // sub-page random access for PFOR is out of scope here -- see the PR
  // description / docs for the investigation.
  //
  // `src` must point at the very start of the compressed stream (its first
  // word is the value count covered by the block-packed region, written by
  // FastBinaryPacking::encodeArray -- a multiple of kBlockSize). Positions
  // >= blockPackedCount(src) fall in the composite codec's VariableByte tail
  // and are not covered by decompressBlock; callers must fall back to a full
  // decompress() for those.
  static constexpr u32 kBlockSize = 128;
  u32 blockPackedCount(const data_t* src) const;
  // Decodes exactly one block of kBlockSize values (block_index within the
  // block-packed region, 0-based) into `dest`. Cost is O(block_index) to skip
  // preceding blocks' header words (a word read per block, not a decode) plus
  // O(kBlockSize) to unpack the target block -- not O(count).
  void decompressBlock(const data_t* src, u32 block_index, data_t* dest) const;
  // -------------------------------------------------------------------------------------
  static void applyDelta(data_t* src, size_t count);
  static void revertDelta(data_t* src, size_t count);
  // -------------------------------------------------------------------------------------
 private:
  struct impl;
  std::unique_ptr<impl> pImpl;
};
// -------------------------------------------------------------------------------------
using FPFor = LemiereImpl<FastPForCodec::FPF>;
using FBPImpl = LemiereImpl<FastPForCodec::FBP>;
// -------------------------------------------------------------------------------------
