#pragma once
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "scheme/SchemeConfig.hpp"
#include "scheme/SchemeType.hpp"
// -------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
// -------------------------------------------------------------------------------------
// The split plan: which bit ranges a value is decomposed into, plus the
// machinery for describing one textually and for forcing one from outside.
// -------------------------------------------------------------------------------------
namespace btrblocks::subintsplit {
// -------------------------------------------------------------------------------------
struct SegmentPlan {
  // Inclusive bit range, counted from the least significant bit.
  int bitStart{0};
  int bitEnd{0};
  // What the planner predicted would compress this range best. The section's
  // scheme is chosen independently by the picker at encode time; keeping the
  // prediction lets the two be compared.
  IntegerSchemeType predictedScheme{IntegerSchemeType::UNCOMPRESSED};
  // Estimated cost in bits for the full stream.
  double cost{0.0};

  int width() const { return bitEnd - bitStart + 1; }
};
// -------------------------------------------------------------------------------------
struct SplitPlan {
  std::vector<SegmentPlan> segments;
  double totalCost{0.0};
};
// -------------------------------------------------------------------------------------
// What the last encode chose, for diagnostics.
//
// Three things here cannot be recovered from the encoded bytes afterwards. A
// section's size is not in the header -- descriptors carry offsets but no
// total, so a reader cannot determine the final section's length. The scheme
// the planner *predicted* is discarded once the picker has made its own
// choice. And the time spent planning is not a property of the output at all.
//
// Kept out of the wire format deliberately: this is diagnostic, and paying
// header bytes on every column to answer questions only a benchmark asks would
// be the wrong trade.
struct SectionReport {
  u8 bit_start{0};
  u8 bit_end{0};
  // What the cost models expected to win for this range...
  IntegerSchemeType predicted{IntegerSchemeType::UNCOMPRESSED};
  // ...against what the picker actually chose. Divergence is the signal that
  // the cost models are mis-modelling something.
  IntegerSchemeType actual{IntegerSchemeType::UNCOMPRESSED};
  u32 bytes{0};
};
// -------------------------------------------------------------------------------------
struct PlanReport {
  bool valid{false};
  bool raw_fallback{false};
  // Boundaries came from an override rather than the planner, so the per-section
  // `predicted` fields hold no prediction and must not be read as one.
  bool forced_boundaries{false};
  u8 value_bits{0};
  u32 tuple_count{0};
  u32 total_bytes{0};
  // Sampling plus the dynamic program, excluding section compression. Encode
  // cost is dominated by one or the other, and the totals alone cannot say which.
  double plan_ms{0.0};
  std::vector<SectionReport> sections;
};
// -------------------------------------------------------------------------------------
// The most recent encode on this thread. Overwritten per chunk, so read it
// immediately after the compress whose plan you want.
inline PlanReport& lastPlanReport() {
  static thread_local PlanReport report;
  return report;
}
// -------------------------------------------------------------------------------------
// Widest section the sub-scheme pool can safely handle.
//
// A section is handed to the ordinary integer scheme picker as INTEGER, so 32
// is the ceiling. 32 is also the right default: the default schemes are all
// bit-exact on arbitrary 32-bit patterns, including ones with the sign bit set
// (UNCOMPRESSED and ONE_VALUE copy raw bytes, BP/PFOR reinterpret to unsigned
// for FastPFor, DICT's ordering is self-consistent, RLE only compares equality).
//
// FOR and the truncation schemes are the exceptions: they do signed arithmetic
// on the value (`src[i] - stats.min`), which overflows for a full-width section.
// So the cap drops to 31 whenever one of those is enabled, which keeps every
// section value non-negative. This is derived from the enabled set rather than
// from a build flag because the schemes are enabled at runtime.
inline int effectiveMaxSectionBits() {
  const auto& schemes = BtrBlocksConfig::get().integers.schemes;
  const bool signSensitive = schemes.isEnabled(IntegerSchemeType::FOR) ||
                             schemes.isEnabled(IntegerSchemeType::TRUNCATION_8) ||
                             schemes.isEnabled(IntegerSchemeType::TRUNCATION_16);
  const int configured = SchemeConfig::get().integers.subintsplit.max_section_bits;
  return std::min(configured, signSensitive ? 31 : 32);
}
// -------------------------------------------------------------------------------------
// "0-20;21-40;41-63" -- inclusive ranges, ascending, contiguous, covering
// exactly [0, totalBits).
inline std::string serializeSplitBoundaries(const std::vector<SegmentPlan>& segments) {
  std::string out;
  for (std::size_t i = 0; i < segments.size(); i++) {
    if (i > 0) {
      out += ';';
    }
    out += std::to_string(segments[i].bitStart);
    out += '-';
    out += std::to_string(segments[i].bitEnd);
  }
  return out;
}
// -------------------------------------------------------------------------------------
// Parse the above. Returns false if the text is malformed or the ranges do not
// tile [0, totalBits) exactly.
bool parseSplitBoundaries(const std::string& text, int totalBits, std::vector<SegmentPlan>& out);
// -------------------------------------------------------------------------------------
// Forced boundaries, for benchmarking a specific split against the planner's.
// When set, the planner is bypassed entirely and these ranges are used as-is.
// Thread-local, mirroring how EnforceScheme overrides scheme selection.
std::vector<SegmentPlan>*& forcedSplitBoundaries();
// -------------------------------------------------------------------------------------
struct EnforceSplitBoundaries {
  explicit EnforceSplitBoundaries(std::vector<SegmentPlan> segments) : owned_(std::move(segments)) {
    forcedSplitBoundaries() = &owned_;
  }
  ~EnforceSplitBoundaries() { forcedSplitBoundaries() = nullptr; }

  EnforceSplitBoundaries(const EnforceSplitBoundaries&) = delete;
  EnforceSplitBoundaries& operator=(const EnforceSplitBoundaries&) = delete;

 private:
  std::vector<SegmentPlan> owned_;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::subintsplit
// -------------------------------------------------------------------------------------
