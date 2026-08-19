#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
// -------------------------------------------------------------------------------------
// Stream sampler for the SubIntSplit split planner.
//
// Values are widened to uint64_t (raw bit pattern, zero-extended) so the
// planner is width-agnostic. Output goes into a caller-owned vector so the
// allocation survives across calls.
// -------------------------------------------------------------------------------------
namespace btrblocks::subintsplit {
// -------------------------------------------------------------------------------------
struct SamplerConfig {
  // Upper bound on samples drawn. The planner runs an O(bits^2 * sampleSize)
  // grid over these, so raising it sharpens the cost estimates only marginally
  // while making planning substantially slower -- especially at 64 bits.
  std::size_t maxSamples{2048};
  // Samples are drawn as contiguous blocks of this many values. Blocks matter
  // for run-length statistics: a field that only changes every few thousand
  // rows (a snowflake timestamp, say) looks like noise under stride sampling
  // and like the long runs it really has under block sampling.
  // 0 selects uniform stride sampling instead.
  std::size_t blockSize{128};
};
// -------------------------------------------------------------------------------------
// Draw up to cfg.maxSamples values from `values` into `out`.
//
// When `nullmap` is non-null, rows it marks absent are skipped. Null slots hold
// whatever was in the input buffer, and letting that garbage into the sample
// would corrupt the split plan for a column with many nulls.
template <typename T>
void sampleIntoU64(const T* values,
                   std::size_t count,
                   const BITMAP* nullmap,
                   std::vector<uint64_t>& out,
                   const SamplerConfig& cfg) {
  static_assert(sizeof(T) <= 8, "sampled type must fit in uint64_t");

  out.clear();
  if (count == 0) {
    return;
  }

  const std::size_t target = std::min(cfg.maxSamples > 0 ? cfg.maxSamples : count, count);
  out.reserve(target);

  const auto append = [&](std::size_t index) {
    if (nullmap != nullptr && nullmap[index] == 0) {
      return;
    }
    uint64_t bits = 0;
    std::memcpy(&bits, &values[index], sizeof(T));
    out.push_back(bits);
  };

  if (cfg.blockSize > 0) {
    const std::size_t blocks = std::max<std::size_t>(1, target / cfg.blockSize);
    const std::size_t stride = std::max<std::size_t>(1, count / blocks);
    for (std::size_t b = 0; b < blocks && out.size() < target; b++) {
      const std::size_t start = b * stride;
      const std::size_t end = std::min(start + cfg.blockSize, count);
      for (std::size_t i = start; i < end && out.size() < target; i++) {
        append(i);
      }
    }
  } else {
    const std::size_t stride = std::max<std::size_t>(1, count / target);
    for (std::size_t i = 0; i < count && out.size() < target; i += stride) {
      append(i);
    }
  }

  // A column that is entirely null within the sampled blocks still needs
  // something to plan against; fall back to the raw bit patterns.
  if (out.empty()) {
    const std::size_t stride = std::max<std::size_t>(1, count / target);
    for (std::size_t i = 0; i < count && out.size() < target; i += stride) {
      uint64_t bits = 0;
      std::memcpy(&bits, &values[i], sizeof(T));
      out.push_back(bits);
    }
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::subintsplit
// -------------------------------------------------------------------------------------
