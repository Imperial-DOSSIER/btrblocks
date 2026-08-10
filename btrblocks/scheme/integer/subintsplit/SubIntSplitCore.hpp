#pragma once
// -------------------------------------------------------------------------------------
#include "cache/ThreadCache.hpp"
#include "common/Exceptions.hpp"
#include "common/SIMD.hpp"
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/SchemeConfig.hpp"
#include "scheme/integer/subintsplit/CostModels.hpp"
#include "scheme/integer/subintsplit/Plan.hpp"
#include "scheme/integer/subintsplit/Sampler.hpp"
#include "scheme/integer/subintsplit/Selector.hpp"
// -------------------------------------------------------------------------------------
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
// -------------------------------------------------------------------------------------
// Encoding and decoding for SubIntSplit, templated on the unsigned value type
// so 32- and 64-bit columns share one implementation.
//
// Only the extraction and accumulation loops are width-aware. Every section is
// at most 32 bits, so once extracted it is an ordinary INTEGER stream that the
// existing scheme picker compresses -- which is how a 64-bit column reuses the
// 32-bit scheme pool without needing a 64-bit one.
// -------------------------------------------------------------------------------------
namespace btrblocks::subintsplit {
// -------------------------------------------------------------------------------------
// Wire format:
//
//   SubIntSplitHeader
//   SectionDescriptor  x section_count
//   payload            x section_count   (at the recorded offsets)
//
// Packed because dest points into the middle of a compression output block and
// is never aligned. Sections are stored least-significant range first.
struct __attribute__((packed)) SubIntSplitHeader {
  u8 format_version;
  u8 section_count;
  u8 value_bits;  // 32 or 64
  // Reserved for a sub-stream reordering transform (BWT, MTF, ...). 0 is
  // "none"; the field exists so one can be added without a format break.
  u8 reorderer_id;
  u8 data[];
};
// -------------------------------------------------------------------------------------
struct __attribute__((packed)) SectionDescriptor {
  u8 bit_start;  // inclusive, from the least significant bit
  u8 bit_end;    // inclusive
  u8 scheme_code;
  u8 padding;
  u32 offset;  // from the start of the payload area
};
// -------------------------------------------------------------------------------------
inline constexpr u8 kFormatVersion = 1;
inline constexpr u8 kReordererNone = 0;
// Sentinel scheme code for the uncompressed fallback: the payload is the raw
// value array. Distinct from every real code, and equal to autoScheme(), which
// is never stored as one.
inline constexpr u8 kRawSectionScheme = 255;
// -------------------------------------------------------------------------------------
// Marks the section loop as nested for the duration of its scope.
//
// Sections must never themselves be compressed with SubIntSplit: splitting an
// already-split section is meaningless, and it recurses -- the picker asks each
// candidate scheme to estimate itself, which re-enters encode() and clobbers
// the shared scratch buffers. SubIntSplit::expectedCompressionRatio declines
// when it sees itself nested, but "nested" is measured by cascade depth, and
// entering encode() directly rather than through the picker starts a level
// shallower than entering it as a chosen scheme. Raising the level explicitly
// makes the guard fire either way.
//
// Raising it also suppresses the picker's top-level ONE_VALUE special case,
// which is why encode() forces ONE_VALUE itself for constant sections.
struct NestedCompressionScope {
  NestedCompressionScope() { ThreadCache::get().compression_level++; }
  ~NestedCompressionScope() { ThreadCache::get().compression_level--; }

  NestedCompressionScope(const NestedCompressionScope&) = delete;
  NestedCompressionScope& operator=(const NestedCompressionScope&) = delete;
};
// -------------------------------------------------------------------------------------
template <typename UT>
class SubIntSplitCore {
 public:
  static_assert(std::is_same<UT, u32>::value || std::is_same<UT, u64>::value,
                "SubIntSplit supports 32- and 64-bit values");

  static constexpr int kValueBits = static_cast<int>(sizeof(UT) * 8);

  // -----------------------------------------------------------------------------------
  static u32 encode(const UT* src,
                    const BITMAP* nullmap,
                    u8* dest,
                    u32 tuple_count,
                    u8 allowed_cascading_level) {
    auto& header = *reinterpret_cast<SubIntSplitHeader*>(dest);
    header.format_version = kFormatVersion;
    header.value_bits = static_cast<u8>(kValueBits);
    header.reorderer_id = kReordererNone;

    if (tuple_count == 0) {
      header.section_count = 1;
      auto& descriptor = *reinterpret_cast<SectionDescriptor*>(header.data);
      descriptor.bit_start = 0;
      descriptor.bit_end = static_cast<u8>(kValueBits - 1);
      descriptor.scheme_code = kRawSectionScheme;
      descriptor.padding = 0;
      descriptor.offset = 0;
      return sizeof(SubIntSplitHeader) + sizeof(SectionDescriptor);
    }

    const auto segments = planSplit(src, nullmap, tuple_count);
    const auto section_count = static_cast<u8>(segments.size());

    const u32 header_size = sizeof(SubIntSplitHeader) + section_count * sizeof(SectionDescriptor);
    auto* descriptors = reinterpret_cast<SectionDescriptor*>(header.data);
    u8* payload = dest + header_size;

    // Every section costs a full INTEGER per value before its sub-scheme runs,
    // so a plan whose sections all resist compression can outgrow the raw
    // input. Track the running size and abandon the plan if it does; the caller
    // hands us a buffer sized for the whole datablock, so silently overrunning
    // it would corrupt other columns.
    const u32 raw_size = tuple_count * sizeof(UT);

    std::vector<INTEGER>& section_values = sectionScratch(tuple_count);
    u32 written = 0;

    NestedCompressionScope nested;
    for (u8 s = 0; s < section_count; s++) {
      const auto& segment = segments[s];
      const int width = segment.width();
      const UT mask = maskFor(width);

      // Extract the section, tracking constancy as we go: the picker skips
      // ONE_VALUE for nested streams, so a constant section (a snowflake's sign
      // bit, say) would otherwise pay per-value costs for a single value.
      INTEGER first = 0;
      bool constant = true;
      for (u32 i = 0; i < tuple_count; i++) {
        const auto value =
            static_cast<INTEGER>(static_cast<u32>((src[i] >> segment.bitStart) & mask));
        section_values[i] = value;
        if (i == 0) {
          first = value;
        } else if (value != first) {
          constant = false;
        }
      }

      auto& descriptor = descriptors[s];
      descriptor.bit_start = static_cast<u8>(segment.bitStart);
      descriptor.bit_end = static_cast<u8>(segment.bitEnd);
      descriptor.padding = 0;
      descriptor.offset = written;

      u32 used = 0;
      u8 scheme_code = 0;
      IntegerSchemePicker::compress(section_values.data(), nullptr, payload + written, tuple_count,
                                    allowed_cascading_level - 1, used, scheme_code,
                                    constant ? CB(IntegerSchemeType::ONE_VALUE) : autoScheme(),
                                    "sis_section");
      descriptor.scheme_code = scheme_code;
      written += used;

      if (written > raw_size) {
        return encodeRaw(dest, src, tuple_count);
      }
    }

    header.section_count = section_count;
    return header_size + written;
  }

  // -----------------------------------------------------------------------------------
  static void decode(UT* dest, const u8* src, u32 tuple_count, u32 level) {
    const auto& header = *reinterpret_cast<const SubIntSplitHeader*>(src);
    validate(header);
    if (tuple_count == 0) {
      return;
    }

    const auto* descriptors = reinterpret_cast<const SectionDescriptor*>(header.data);
    const u8* payload =
        src + sizeof(SubIntSplitHeader) + header.section_count * sizeof(SectionDescriptor);

    if (isRaw(header, descriptors)) {
      std::memcpy(dest, payload, static_cast<std::size_t>(tuple_count) * sizeof(UT));
      return;
    }

    // One scratch buffer, reused across sections. BtrBlocks sub-schemes decode
    // a whole stream at a time -- there is no range or offset variant -- so a
    // plan with N sections makes N passes over the column no matter what. See
    // docs/subintsplit.md on what that costs.
    INTEGER* scratch = decodeScratch(tuple_count, level);

    for (u8 s = 0; s < header.section_count; s++) {
      const auto& descriptor = descriptors[s];
      validate(descriptor, header.value_bits);

      auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(descriptor.scheme_code);
      scheme.decompress(scratch, nullptr, payload + descriptor.offset, tuple_count, level + 1);

      const int shift = descriptor.bit_start;
      const UT mask = maskFor(descriptor.bit_end - descriptor.bit_start + 1);
      if (s == 0) {
        for (u32 i = 0; i < tuple_count; i++) {
          dest[i] = (static_cast<UT>(static_cast<u32>(scratch[i])) & mask) << shift;
        }
      } else {
        for (u32 i = 0; i < tuple_count; i++) {
          dest[i] |= (static_cast<UT>(static_cast<u32>(scratch[i])) & mask) << shift;
        }
      }
    }
  }

  // -----------------------------------------------------------------------------------
  // Materialize only the requested rows.
  //
  // Each section is still decoded in full, because BtrBlocks sub-schemes expose
  // no range or offset decode: BP and PFOR decompress a whole FastPFor array,
  // DICT's codes are bit-packed, RLE has no run-offset index. So this saves the
  // accumulation work for unwanted rows, not the sub-scheme work, and is a
  // constant-factor win over decode() rather than an asymptotic one.
  //
  // The honest consequence -- SubIntSplit cannot offer real point access while
  // its sub-schemes have none -- is recorded in docs/subintsplit.md rather than
  // papered over here.
  static void gather(UT* dest,
                     const u8* src,
                     u32 tuple_count,
                     const u32* positions,
                     u32 position_count,
                     u32 level) {
    const auto& header = *reinterpret_cast<const SubIntSplitHeader*>(src);
    validate(header);
    if (tuple_count == 0 || position_count == 0) {
      return;
    }

    const auto* descriptors = reinterpret_cast<const SectionDescriptor*>(header.data);
    const u8* payload =
        src + sizeof(SubIntSplitHeader) + header.section_count * sizeof(SectionDescriptor);

    if (isRaw(header, descriptors)) {
      const auto* values = reinterpret_cast<const UT*>(payload);
      for (u32 i = 0; i < position_count; i++) {
        dest[i] = values[positions[i]];
      }
      return;
    }

    INTEGER* scratch = decodeScratch(tuple_count, level);

    for (u8 s = 0; s < header.section_count; s++) {
      const auto& descriptor = descriptors[s];
      validate(descriptor, header.value_bits);

      auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(descriptor.scheme_code);
      scheme.decompress(scratch, nullptr, payload + descriptor.offset, tuple_count, level + 1);

      const int shift = descriptor.bit_start;
      const UT mask = maskFor(descriptor.bit_end - descriptor.bit_start + 1);
      if (s == 0) {
        for (u32 i = 0; i < position_count; i++) {
          dest[i] = (static_cast<UT>(static_cast<u32>(scratch[positions[i]])) & mask) << shift;
        }
      } else {
        for (u32 i = 0; i < position_count; i++) {
          dest[i] |= (static_cast<UT>(static_cast<u32>(scratch[positions[i]])) & mask) << shift;
        }
      }
    }
  }

  // -----------------------------------------------------------------------------------
  // Largest encoding this can produce for `tuple_count` values: the raw
  // fallback, which is what encode() falls back to whenever a plan would be
  // bigger. The section-count cap is what makes this bound finite.
  static u64 maxCompressedSize(u32 tuple_count) {
    const auto max_sections = SchemeConfig::get().integers.subintsplit.max_sections;
    const u64 header =
        sizeof(SubIntSplitHeader) + static_cast<u64>(max_sections) * sizeof(SectionDescriptor);
    // A section is abandoned as soon as the running total passes the raw size,
    // so at most one section's worth of overshoot is ever written.
    const u64 overshoot = static_cast<u64>(tuple_count) * sizeof(INTEGER) + 1024;
    return header + static_cast<u64>(tuple_count) * sizeof(UT) + overshoot;
  }

  // -----------------------------------------------------------------------------------
  // Describes the split and each section's chosen scheme, e.g.
  //   SUB_INT_SPLIT[0-9;10-22;23-63] -> ([0-9] BP) -> ([10-22] ONE_VALUE) ...
  static std::string describe(const u8* src, const std::string& self) {
    const auto& header = *reinterpret_cast<const SubIntSplitHeader*>(src);
    if (header.format_version != kFormatVersion) {
      return self + "[unknown format version]";
    }
    const auto* descriptors = reinterpret_cast<const SectionDescriptor*>(header.data);
    const u8* payload =
        src + sizeof(SubIntSplitHeader) + header.section_count * sizeof(SectionDescriptor);

    if (isRaw(header, descriptors)) {
      return self + "[raw]";
    }

    std::string boundaries;
    std::string children;
    for (u8 s = 0; s < header.section_count; s++) {
      const auto& descriptor = descriptors[s];
      const std::string range =
          std::to_string(descriptor.bit_start) + "-" + std::to_string(descriptor.bit_end);
      if (s > 0) {
        boundaries += ';';
      }
      boundaries += range;

      auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(descriptor.scheme_code);
      children +=
          "\n\t-> ([" + range + "] section) " + scheme.fullDescription(payload + descriptor.offset);
    }
    return self + "[" + boundaries + "]" + children;
  }

  // -----------------------------------------------------------------------------------
  static u8 sectionCount(const u8* src) {
    return reinterpret_cast<const SubIntSplitHeader*>(src)->section_count;
  }

 private:
  // -----------------------------------------------------------------------------------
  static UT maskFor(int width) { return width >= kValueBits ? ~UT{0} : ((UT{1} << width) - 1); }
  // -----------------------------------------------------------------------------------
  static bool isRaw(const SubIntSplitHeader& header, const SectionDescriptor* descriptors) {
    return header.section_count == 1 && descriptors[0].scheme_code == kRawSectionScheme;
  }
  // -----------------------------------------------------------------------------------
  static void validate(const SubIntSplitHeader& header) {
    // The header is on-disk format, so a corrupt or future one must be
    // rejected rather than misparsed.
    if (header.format_version != kFormatVersion) {
      throw Generic_Exception("SubIntSplit: unsupported format version " +
                              std::to_string(header.format_version));
    }
    if (header.value_bits != kValueBits) {
      throw Generic_Exception("SubIntSplit: data was encoded at " +
                              std::to_string(header.value_bits) + " bits, decoded at " +
                              std::to_string(kValueBits));
    }
    if (header.section_count == 0) {
      throw Generic_Exception("SubIntSplit: zero sections");
    }
  }
  // -----------------------------------------------------------------------------------
  static void validate(const SectionDescriptor& descriptor, u8 value_bits) {
    if (descriptor.bit_end < descriptor.bit_start || descriptor.bit_end >= value_bits) {
      throw Generic_Exception("SubIntSplit: corrupt section bit range");
    }
  }
  // -----------------------------------------------------------------------------------
  // Fallback: store the values verbatim. Bounds the encoded size at the raw
  // size plus the header, unconditionally.
  static u32 encodeRaw(u8* dest, const UT* src, u32 tuple_count) {
    auto& header = *reinterpret_cast<SubIntSplitHeader*>(dest);
    header.format_version = kFormatVersion;
    header.section_count = 1;
    header.value_bits = static_cast<u8>(kValueBits);
    header.reorderer_id = kReordererNone;

    auto& descriptor = *reinterpret_cast<SectionDescriptor*>(header.data);
    descriptor.bit_start = 0;
    descriptor.bit_end = static_cast<u8>(kValueBits - 1);
    descriptor.scheme_code = kRawSectionScheme;
    descriptor.padding = 0;
    descriptor.offset = 0;

    const u32 header_size = sizeof(SubIntSplitHeader) + sizeof(SectionDescriptor);
    const auto bytes = static_cast<std::size_t>(tuple_count) * sizeof(UT);
    std::memcpy(dest + header_size, src, bytes);
    return header_size + static_cast<u32>(bytes);
  }
  // -----------------------------------------------------------------------------------
  static std::vector<SegmentPlan> planSplit(const UT* src, const BITMAP* nullmap, u32 tuple_count) {
    if (auto* forced = forcedSplitBoundaries(); forced != nullptr && !forced->empty()) {
      return *forced;
    }

    const auto& tuning = SchemeConfig::get().integers.subintsplit;
    SamplerConfig sampler_cfg;
    sampler_cfg.maxSamples = tuning.sample_size;
    sampler_cfg.blockSize = tuning.sample_block_size;

    thread_local std::vector<uint64_t> samples;
    sampleIntoU64(src, tuple_count, nullmap, samples, sampler_cfg);

    auto plan = selectSplits(samples, kValueBits, tuple_count, defaultCostModels(),
                             defaultSelectorConfig());
    return plan.segments;
  }
  // -----------------------------------------------------------------------------------
  static std::vector<INTEGER>& sectionScratch(u32 tuple_count) {
    thread_local std::vector<INTEGER> scratch;
    if (scratch.size() < tuple_count) {
      scratch.resize(tuple_count);
    }
    return scratch;
  }
  // -----------------------------------------------------------------------------------
  static INTEGER* decodeScratch(u32 tuple_count, u32 level) {
    // The SIMD slack is required: TRLE stores whole 256-bit vectors past the
    // logical end of its output.
    thread_local std::vector<std::vector<INTEGER>> scratch;
    return get_level_data(scratch, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::subintsplit
// -------------------------------------------------------------------------------------
