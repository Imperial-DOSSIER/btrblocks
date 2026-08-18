// -------------------------------------------------------------------------------------
#include "FastPFOR.hpp"
// -------------------------------------------------------------------------------------
#include "common/Exceptions.hpp"
#include "common/SIMD.hpp"
// -------------------------------------------------------------------------------------
// fastpfor
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#include <headers/blockpacking.h>
#include <headers/compositecodec.h>
#include <headers/deltautil.h>
#include <headers/simdfastpfor.h>
#include <headers/variablebyte.h>
#pragma GCC diagnostic pop
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
template <>
struct LemiereImpl<FastPForCodec::FPF>::impl {
  // using codec_t = BTR_IFELSESIMD(FastPForLib::SIMDFastPFor<8>,
  // FastPForLib::FastPFor<8>);
  using codec_t = FastPForLib::SIMDFastPFor<8>;
  FastPForLib::CompositeCodec<codec_t, FastPForLib::VariableByte> codec;
};
// -------------------------------------------------------------------------------------
template <>
struct LemiereImpl<FastPForCodec::FBP>::impl {
  // TODO Adnan did not use SIMDBinaryPacking in the original? ask him why
  FastPForLib::CompositeCodec<FastPForLib::FastBinaryPacking<32>, FastPForLib::VariableByte> codec;
};
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
LemiereImpl<Codec>::LemiereImpl() : pImpl(new LemiereImpl<Codec>::impl) {}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
LemiereImpl<Codec>::~LemiereImpl() = default;
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
u32 LemiereImpl<Codec>::compress(const data_t* src, u32 count, data_t* dest, SIZE& outsize) {
  auto& codec = this->pImpl->codec;
  codec.encodeArray(src, count, dest, outsize);
  return outsize;
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
const typename LemiereImpl<Codec>::data_t* LemiereImpl<Codec>::decompress(const data_t* src,
                                                                          u32 count,
                                                                          data_t* dest,
                                                                          SIZE& outsize) {
  auto& codec = this->pImpl->codec;
  return codec.decodeArray(src, count, dest, outsize);
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
u32 LemiereImpl<Codec>::blockPackedCount(const data_t* src) const {
  if constexpr (Codec == FastPForCodec::FBP) {
    // Written verbatim by FastBinaryPacking<32>::encodeArray as `*out++ = length`.
    return src[0];
  } else {
    throw ::Generic_Exception("blockPackedCount is only supported for FastPForCodec::FBP");
  }
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
void LemiereImpl<Codec>::decompressBlock(const data_t* src, u32 block_index, data_t* dest) const {
  if constexpr (Codec == FastPForCodec::FBP) {
    // Mirrors FastBinaryPacking<32>::decodeArray's loop body (blockpacking.h)
    // for exactly one block, instead of decoding every block from the start.
    // MiniBlockSize=32, HowManyMiniBlocks=4 => kBlockSize=128, matching the
    // FastBinaryPacking<32> instantiation FBPImpl uses (see the `impl` struct
    // above). Each block: one header word packing four 8-bit widths, then
    // Bs[i] packed words per 32-value mini-block.
    const uint32_t* in = src + 1;
    for (u32 b = 0; b < block_index; b++) {
      const uint32_t header = in[0];
      const u32 bs0 = static_cast<uint8_t>(header >> 24);
      const u32 bs1 = static_cast<uint8_t>(header >> 16);
      const u32 bs2 = static_cast<uint8_t>(header >> 8);
      const u32 bs3 = static_cast<uint8_t>(header);
      in += 1 + bs0 + bs1 + bs2 + bs3;
    }
    const uint32_t header = in[0];
    const u32 bs[4] = {
        static_cast<uint8_t>(header >> 24),
        static_cast<uint8_t>(header >> 16),
        static_cast<uint8_t>(header >> 8),
        static_cast<uint8_t>(header),
    };
    ++in;
    for (u32 i = 0; i < 4; i++) {
      FastPForLib::fastunpack(in, dest + i * 32, bs[i]);
      in += bs[i];
    }
  } else {
    throw ::Generic_Exception("decompressBlock is only supported for FastPForCodec::FBP");
  }
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
void LemiereImpl<Codec>::applyDelta(data_t* src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::deltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
void LemiereImpl<Codec>::revertDelta(data_t* src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::inverseDeltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template struct LemiereImpl<FastPForCodec::FPF>;
template struct LemiereImpl<FastPForCodec::FBP>;
// -------------------------------------------------------------------------------------
