// -------------------------------------------------------------------------------------
// SubIntSplit benchmark.
//
// Compares SubIntSplit against the integer codecs BtrBlocks already has, across
// encode time, compression ratio, and the bulk, gather and point decode paths.
//
// The 32-bit arm runs through Relation, Datablock and BtrReader, so it measures
// the real chunked storage path rather than a synthetic one. The 64-bit arm is
// driven directly, because BtrBlocks has no 64-bit scheme hierarchy for it to
// join -- but its sections are still compressed by the ordinary 32-bit pool, so
// the comparison is against the same codecs either way.
//
// Two control arms matter for interpreting the results:
//   - a fixed halves split (0-31;32-63), which isolates what the planner's
//     choice of boundaries is worth from what splitting at all is worth;
//   - uniformly random data, where there is no bit-range structure and
//     splitting should not help.
// -------------------------------------------------------------------------------------
#include "SnowflakeGen.hpp"
#include "Traces.hpp"
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "compression/Datablock.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/SchemePool.hpp"
#include "scheme/integer/SubIntSplit64.hpp"
#include "scheme/integer/subintsplit/Plan.hpp"
#include "storage/Chunk.hpp"
#include "storage/Relation.hpp"
// -------------------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
// -------------------------------------------------------------------------------------
using namespace btrblocks;
using namespace btrblocks::subintsplit_bench;
// -------------------------------------------------------------------------------------
namespace {
// -------------------------------------------------------------------------------------
using Clock = std::chrono::steady_clock;

double millisSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
// -------------------------------------------------------------------------------------
// Median of `repeats` runs. Median rather than mean because a single scheduling
// hiccup should not move the reported number.
double timeMedian(int repeats, const std::function<void()>& body) {
  std::vector<double> samples;
  samples.reserve(repeats);
  for (int i = 0; i < repeats; i++) {
    const auto start = Clock::now();
    body();
    samples.push_back(millisSince(start));
  }
  std::sort(samples.begin(), samples.end());
  return samples[samples.size() / 2];
}
// -------------------------------------------------------------------------------------
// One row per section of a plan: where it sits, what compressed it, and what
// it cost. The run-level CSV reports one total per encoding, which cannot say
// whether a 41-bit timestamp dominates the output or some 3-bit field is being
// wasteful.
struct SectionRow {
  uint32_t index{0};
  uint8_t bit_start{0};
  uint8_t bit_end{0};
  std::string predicted;  // what the cost models expected to win here
  std::string actual;     // what the picker chose
  uint32_t bytes{0};
  double bits_per_value{0.0};
  double share_pct{0.0};  // share of the encoding this section accounts for
};
// -------------------------------------------------------------------------------------
struct Result {
  std::string width;
  std::string dataset;
  std::string codec;
  uint32_t rows{0};
  uint32_t block_size{0};
  uint32_t chunks{0};
  uint64_t encoded_bytes{0};
  double ratio{0.0};
  double encode_ms{0.0};
  double decode_ms{0.0};
  double gather_uniform_ms{0.0};
  uint32_t gather_uniform_chunks{0};
  uint32_t gather_uniform_rows{0};
  double gather_clustered_ms{0.0};
  uint32_t gather_clustered_chunks{0};
  uint32_t gather_clustered_rows{0};
  double point_ms{0.0};
  uint32_t point_count{0};
  std::string plan;
  // Populated only for SubIntSplit encodings.
  double plan_ms{0.0};
  bool raw_fallback{false};
  std::vector<SectionRow> sections;
};
// -------------------------------------------------------------------------------------
// Turns the encoder's report on the last chunk it compressed into rows.
// No-ops for any codec that is not SubIntSplit, which leaves the report invalid.
void collectSections(Result& result) {
  const auto& report = subintsplit::lastPlanReport();
  if (!report.valid) {
    return;
  }
  result.plan_ms = report.plan_ms;
  result.raw_fallback = report.raw_fallback;

  uint32_t total = 0;
  for (const auto& section : report.sections) {
    total += section.bytes;
  }
  uint32_t index = 0;
  for (const auto& section : report.sections) {
    SectionRow row;
    row.index = index++;
    row.bit_start = section.bit_start;
    row.bit_end = section.bit_end;
    // With forced boundaries the planner never ran, so there is no prediction to
    // report. Emitting the default-constructed enum would look like a wrong
    // prediction and skew any accuracy figure computed from this file.
    row.predicted = report.forced_boundaries ? "-" : ConvertSchemeTypeToString(section.predicted);
    row.actual = ConvertSchemeTypeToString(section.actual);
    row.bytes = section.bytes;
    row.bits_per_value = report.tuple_count > 0 ? (8.0 * section.bytes) / report.tuple_count : 0.0;
    row.share_pct = total > 0 ? (100.0 * section.bytes) / total : 0.0;
    result.sections.push_back(row);
  }
}
// -------------------------------------------------------------------------------------
void writeSectionsHeader(std::ostream& out) {
  out << "width,dataset,codec,block_size,rows,section,bit_start,bit_end,bits,predicted,actual,"
         "bytes,bits_per_value,share_pct,plan_ms,raw_fallback\n";
}
// -------------------------------------------------------------------------------------
void writeSectionRows(std::ostream& out, const Result& r) {
  for (const auto& s : r.sections) {
    out << r.width << ',' << r.dataset << ',' << r.codec << ',' << r.block_size << ',' << r.rows
        << ',' << s.index << ',' << static_cast<int>(s.bit_start) << ','
        << static_cast<int>(s.bit_end) << ',' << (s.bit_end - s.bit_start + 1) << ',' << s.predicted
        << ',' << s.actual << ',' << s.bytes << ',' << s.bits_per_value << ',' << s.share_pct
        << ','
        // Repeated per row so it survives a naive group-by.
        << r.plan_ms << ',' << (r.raw_fallback ? 1 : 0) << '\n';
  }
}
// -------------------------------------------------------------------------------------
std::string csvEscape(const std::string& text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"') {
      out += "\"\"";
    } else if (c == '\n' || c == '\t') {
      out += ' ';
    } else {
      out += c;
    }
  }
  out += '"';
  return out;
}
// -------------------------------------------------------------------------------------
void writeCsvHeader(std::ostream& out) {
  out << "width,dataset,codec,rows,block_size,chunks,encoded_bytes,ratio,encode_ms,decode_ms,"
         "gather_uniform_ms,gather_uniform_chunks,gather_uniform_rows,gather_clustered_ms,"
         "gather_clustered_chunks,gather_clustered_rows,point_ms,point_count,plan\n";
}
// -------------------------------------------------------------------------------------
void writeCsvRow(std::ostream& out, const Result& r) {
  out << r.width << ',' << r.dataset << ',' << r.codec << ',' << r.rows << ',' << r.block_size
      << ',' << r.chunks << ',' << r.encoded_bytes << ',' << r.ratio << ',' << r.encode_ms << ','
      << r.decode_ms << ',' << r.gather_uniform_ms << ',' << r.gather_uniform_chunks << ','
      << r.gather_uniform_rows << ',' << r.gather_clustered_ms << ',' << r.gather_clustered_chunks
      << ',' << r.gather_clustered_rows << ',' << r.point_ms << ',' << r.point_count << ','
      << csvEscape(r.plan) << '\n';
}
// -------------------------------------------------------------------------------------
// A codec under test: a top-level scheme, optionally with a forced split.
struct Codec {
  std::string name;
  IntegerSchemeType scheme{IntegerSchemeType::UNCOMPRESSED};
  bool automatic{false};          // let the picker choose
  std::string forced_boundaries;  // empty means let the planner choose
  // Takes SubIntSplit out of the pool, so an automatic codec measures what
  // BtrBlocks would do without this change at all.
  bool exclude_subintsplit{false};
};
// -------------------------------------------------------------------------------------
// Same shape as Codec, for Integer64SchemeType. A separate struct rather than
// a template: the two enums are unrelated types and every call site already
// knows which width it is dealing with.
struct Codec64 {
  std::string name;
  Integer64SchemeType scheme{Integer64SchemeType::UNCOMPRESSED};
  bool automatic{false};
  std::string forced_boundaries;
  bool exclude_subintsplit{false};
};
// -------------------------------------------------------------------------------------
// Removes SubIntSplit from the enabled set for the duration of a scope.
class ScopedSchemeSet {
 public:
  explicit ScopedSchemeSet(bool exclude_subintsplit)
      : saved_(BtrBlocksConfig::get().integers.schemes) {
    if (!exclude_subintsplit) {
      return;
    }
    active_ = true;
    BtrBlocksConfig::get().integers.schemes.disable(IntegerSchemeType::SUB_INT_SPLIT);
    SchemePool::refresh();
  }
  ~ScopedSchemeSet() {
    if (active_) {
      BtrBlocksConfig::get().integers.schemes = saved_;
      SchemePool::refresh();
    }
  }

 private:
  IntegerSchemeSet saved_;
  bool active_{false};
};
// -------------------------------------------------------------------------------------
// Same as ScopedSchemeSet, for the 64-bit scheme set.
class ScopedSchemeSet64 {
 public:
  explicit ScopedSchemeSet64(bool exclude_subintsplit)
      : saved_(BtrBlocksConfig::get().integers64.schemes) {
    if (!exclude_subintsplit) {
      return;
    }
    active_ = true;
    BtrBlocksConfig::get().integers64.schemes.disable(Integer64SchemeType::SUB_INT_SPLIT);
    SchemePool::refresh();
  }
  ~ScopedSchemeSet64() {
    if (active_) {
      BtrBlocksConfig::get().integers64.schemes = saved_;
      SchemePool::refresh();
    }
  }

 private:
  Integer64SchemeSet saved_;
  bool active_{false};
};
// -------------------------------------------------------------------------------------
// Applies a codec's forced split for the duration of a scope, if it has one.
class ScopedBoundaries {
 public:
  ScopedBoundaries(const std::string& text, int value_bits) {
    if (text.empty()) {
      return;
    }
    std::vector<subintsplit::SegmentPlan> segments;
    if (!subintsplit::parseSplitBoundaries(text, value_bits, segments)) {
      throw Generic_Exception("invalid forced boundaries: " + text);
    }
    enforcer_ = std::make_unique<subintsplit::EnforceSplitBoundaries>(std::move(segments));
  }

 private:
  std::unique_ptr<subintsplit::EnforceSplitBoundaries> enforcer_;
};
// -------------------------------------------------------------------------------------
// ---------------------------- 32-bit arm ----------------------------------------------
// -------------------------------------------------------------------------------------
Result run32(const std::string& dataset_name,
             const std::vector<INTEGER>& data,
             const Codec& codec,
             uint32_t block_size,
             int repeats) {
  Result result;
  result.width = "32";
  result.dataset = dataset_name;
  result.codec = codec.name;
  result.rows = static_cast<uint32_t>(data.size());
  result.block_size = block_size;

  BtrBlocksConfig::get().block_size = block_size;
  ScopedSchemeSet scheme_set(codec.exclude_subintsplit);

  Relation relation;
  {
    Vector<INTEGER> column(static_cast<u64>(data.size()));
    for (std::size_t i = 0; i < data.size(); i++) {
      column[i] = data[i];
    }
    relation.addColumn({"ids", std::move(column)});
  }
  const auto ranges = relation.getRanges(SplitStrategy::SEQUENTIAL, 999999);
  result.chunks = static_cast<uint32_t>(ranges.size());

  // ---- encode -------------------------------------------------------------------
  std::vector<std::vector<u8>> compressed(ranges.size());
  const auto compressAll = [&]() {
    ScopedBoundaries boundaries(codec.forced_boundaries, 32);
    for (std::size_t chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
      // The override is consumed by the first compress that sees it, so it has
      // to be re-armed for every chunk.
      if (!codec.automatic) {
        BtrBlocksConfig::get().integers.override_scheme = codec.scheme;
      }
      auto input_chunk = relation.getInputChunk(ranges[chunk_i], chunk_i, 0);
      compressed[chunk_i] = Datablock::compress(input_chunk);
    }
    BtrBlocksConfig::get().integers.override_scheme = static_cast<IntegerSchemeType>(autoScheme());
  };
  result.encode_ms = timeMedian(repeats, compressAll);

  uint64_t encoded_bytes = 0;
  for (const auto& chunk : compressed) {
    encoded_bytes += chunk.size();
  }
  result.encoded_bytes = encoded_bytes;
  result.ratio = static_cast<double>(data.size() * sizeof(INTEGER)) /
                 static_cast<double>(std::max<uint64_t>(encoded_bytes, 1));

  // ---- section breakdown --------------------------------------------------------
  // One more compress of chunk 0, outside the timed loop. Each chunk overwrites
  // the encoder's report, so without this the sections would describe the last
  // chunk while the plan column describes the first.
  {
    subintsplit::lastPlanReport().valid = false;
    ScopedBoundaries boundaries(codec.forced_boundaries, 32);
    if (!codec.automatic) {
      BtrBlocksConfig::get().integers.override_scheme = codec.scheme;
    }
    auto input_chunk = relation.getInputChunk(ranges[0], 0, 0);
    Datablock::compress(input_chunk);
    BtrBlocksConfig::get().integers.override_scheme = static_cast<IntegerSchemeType>(autoScheme());
    collectSections(result);
  }

  // ---- assemble a column part so the read path is the real one ------------------
  ColumnPart part;
  for (auto& chunk : compressed) {
    part.addCompressedChunk(std::move(chunk));
  }
  const std::string path = "subintsplit_bench_column.btr";
  part.writeToDisk(path);

  std::vector<char> file_contents;
  Utils::readFileToMemory(path, file_contents);
  BtrReader reader(file_contents.data());

  {
    std::vector<u8> scratch;
    reader.readColumn(scratch, 0);
    result.plan = reader.getSchemeDescription(0);
  }

  // ---- bulk decode --------------------------------------------------------------
  std::vector<INTEGER> decoded(data.size());
  const auto decodeAll = [&]() {
    std::size_t offset = 0;
    std::vector<u8> scratch;
    for (u32 chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
      reader.readColumn(scratch, chunk_i);
      const auto tuple_count = reader.getTupleCount(chunk_i);
      std::memcpy(decoded.data() + offset, scratch.data(), tuple_count * sizeof(INTEGER));
      offset += tuple_count;
    }
  };
  result.decode_ms = timeMedian(repeats, decodeAll);

  for (std::size_t i = 0; i < data.size(); i++) {
    if (decoded[i] != data[i]) {
      throw Generic_Exception("decode mismatch at row " + std::to_string(i) + " for codec " +
                              codec.name);
    }
  }

  // ---- gather -------------------------------------------------------------------
  const auto measureGather = [&](const std::vector<uint32_t>& positions, double& ms,
                                 uint32_t& chunks_touched, uint32_t& rows) {
    if (positions.empty()) {
      return;
    }
    rows = static_cast<uint32_t>(positions.size());
    std::vector<INTEGER> gathered(positions.size());
    u32 touched = 0;
    ms = timeMedian(repeats, [&]() {
      reader.gatherColumn(gathered.data(), positions.data(), static_cast<u32>(positions.size()),
                          &touched);
    });
    chunks_touched = touched;
    for (std::size_t i = 0; i < positions.size(); i++) {
      if (gathered[i] != data[positions[i]]) {
        throw Generic_Exception("gather mismatch for codec " + codec.name);
      }
    }
  };

  const auto rows32 = static_cast<uint32_t>(data.size());
  measureGather(uniformTrace(rows32, 4096, 7), result.gather_uniform_ms,
                result.gather_uniform_chunks, result.gather_uniform_rows);
  // Low selectivity with long runs, so whole chunks go untouched -- the regime
  // where chunk locality is visible at all.
  measureGather(clusteredTrace(rows32, 0.01, 64.0, 11), result.gather_clustered_ms,
                result.gather_clustered_chunks, result.gather_clustered_rows);

  // ---- point access -------------------------------------------------------------
  const auto point_positions = uniformTrace(rows32, 256, 13);
  result.point_count = static_cast<uint32_t>(point_positions.size());
  result.point_ms = timeMedian(repeats, [&]() {
    for (const auto position : point_positions) {
      volatile INTEGER value = reader.lookupColumn(position);
      (void)value;
    }
  });

  std::remove(path.c_str());
  return result;
}
// -------------------------------------------------------------------------------------
// ---------------------------- 64-bit arm ----------------------------------------------
// -------------------------------------------------------------------------------------
// Mirrors run32() exactly: a real Relation with a BIGINT column, compressed
// chunk-by-chunk through Datablock::compress (which dispatches on
// ColumnType::BIGINT into the registered Integer64Scheme pool), assembled
// into a ColumnPart, written to disk and read back through BtrReader. Now
// that BIGINT is a real column type with a real gatherColumn64/lookupColumn64
// path, there is no reason for the 64-bit arm to hand-chunk or call scheme
// methods directly the way it used to -- doing so measured a synthetic path
// that skipped BtrReader's chunk bucketing and metadata entirely.
Result run64(const std::string& dataset_name,
             const std::vector<s64>& data,
             const Codec64& codec,
             uint32_t block_size,
             int repeats) {
  Result result;
  result.width = "64";
  result.dataset = dataset_name;
  result.codec = codec.name;
  result.rows = static_cast<uint32_t>(data.size());
  result.block_size = block_size;

  BtrBlocksConfig::get().block_size = block_size;
  ScopedSchemeSet64 scheme_set(codec.exclude_subintsplit);

  Relation relation;
  {
    Vector<BIGINT> column(static_cast<u64>(data.size()));
    for (std::size_t i = 0; i < data.size(); i++) {
      column[i] = data[i];
    }
    relation.addColumn({"ids", std::move(column)});
  }
  const auto ranges = relation.getRanges(SplitStrategy::SEQUENTIAL, 999999);
  result.chunks = static_cast<uint32_t>(ranges.size());

  // ---- encode -------------------------------------------------------------------
  std::vector<std::vector<u8>> compressed(ranges.size());
  const auto compressAll = [&]() {
    ScopedBoundaries boundaries(codec.forced_boundaries, 64);
    for (std::size_t chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
      // The override is consumed by the first compress that sees it, so it has
      // to be re-armed for every chunk.
      if (!codec.automatic) {
        BtrBlocksConfig::get().integers64.override_scheme = codec.scheme;
      }
      auto input_chunk = relation.getInputChunk(ranges[chunk_i], chunk_i, 0);
      compressed[chunk_i] = Datablock::compress(input_chunk);
    }
    BtrBlocksConfig::get().integers64.override_scheme =
        static_cast<Integer64SchemeType>(autoScheme());
  };
  result.encode_ms = timeMedian(repeats, compressAll);

  uint64_t encoded_bytes = 0;
  for (const auto& chunk : compressed) {
    encoded_bytes += chunk.size();
  }
  result.encoded_bytes = encoded_bytes;
  result.ratio = static_cast<double>(data.size() * sizeof(s64)) /
                 static_cast<double>(std::max<uint64_t>(encoded_bytes, 1));

  // ---- section breakdown --------------------------------------------------------
  // One more compress of chunk 0, outside the timed loop, same reasoning as run32().
  {
    subintsplit::lastPlanReport().valid = false;
    ScopedBoundaries boundaries(codec.forced_boundaries, 64);
    if (!codec.automatic) {
      BtrBlocksConfig::get().integers64.override_scheme = codec.scheme;
    }
    auto input_chunk = relation.getInputChunk(ranges[0], 0, 0);
    Datablock::compress(input_chunk);
    BtrBlocksConfig::get().integers64.override_scheme =
        static_cast<Integer64SchemeType>(autoScheme());
    collectSections(result);
  }

  // ---- assemble a column part so the read path is the real one ------------------
  ColumnPart part;
  for (auto& chunk : compressed) {
    part.addCompressedChunk(std::move(chunk));
  }
  const std::string path = "subintsplit_bench_column64.btr";
  part.writeToDisk(path);

  std::vector<char> file_contents;
  Utils::readFileToMemory(path, file_contents);
  BtrReader reader(file_contents.data());

  {
    std::vector<u8> scratch;
    reader.readColumn(scratch, 0);
    result.plan = reader.getSchemeDescription(0);
  }

  // ---- bulk decode --------------------------------------------------------------
  std::vector<s64> decoded(data.size());
  const auto decodeAll = [&]() {
    std::size_t offset = 0;
    std::vector<u8> scratch;
    for (u32 chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
      reader.readColumn(scratch, chunk_i);
      const auto tuple_count = reader.getTupleCount(chunk_i);
      std::memcpy(decoded.data() + offset, scratch.data(), tuple_count * sizeof(s64));
      offset += tuple_count;
    }
  };
  result.decode_ms = timeMedian(repeats, decodeAll);

  for (std::size_t i = 0; i < data.size(); i++) {
    if (decoded[i] != data[i]) {
      throw Generic_Exception("decode mismatch at row " + std::to_string(i) + " for codec " +
                              codec.name);
    }
  }

  // ---- gather -------------------------------------------------------------------
  const auto measureGather = [&](const std::vector<uint32_t>& positions, double& ms,
                                 uint32_t& chunks_touched, uint32_t& rows) {
    if (positions.empty()) {
      return;
    }
    rows = static_cast<uint32_t>(positions.size());
    std::vector<s64> gathered(positions.size());
    u32 touched = 0;
    ms = timeMedian(repeats, [&]() {
      reader.gatherColumn64(gathered.data(), positions.data(),
                            static_cast<u32>(positions.size()), &touched);
    });
    chunks_touched = touched;
    for (std::size_t i = 0; i < positions.size(); i++) {
      if (gathered[i] != data[positions[i]]) {
        throw Generic_Exception("gather mismatch for codec " + codec.name);
      }
    }
  };

  const auto rows64 = static_cast<uint32_t>(data.size());
  measureGather(uniformTrace(rows64, 4096, 7), result.gather_uniform_ms,
                result.gather_uniform_chunks, result.gather_uniform_rows);
  // Low selectivity with long runs, so whole chunks go untouched -- the regime
  // where chunk locality is visible at all.
  measureGather(clusteredTrace(rows64, 0.01, 64.0, 11), result.gather_clustered_ms,
                result.gather_clustered_chunks, result.gather_clustered_rows);

  // ---- point access -------------------------------------------------------------
  const auto point_positions = uniformTrace(rows64, 256, 13);
  result.point_count = static_cast<uint32_t>(point_positions.size());
  result.point_ms = timeMedian(repeats, [&]() {
    for (const auto position : point_positions) {
      volatile s64 value = reader.lookupColumn64(position);
      (void)value;
    }
  });

  std::remove(path.c_str());
  return result;
}
// -------------------------------------------------------------------------------------
// Reads up to `max_count` values from a flat little-endian int64 file, as
// produced by parquet_to_i64.py.
//
// Real data arrives as Parquet, which BtrBlocks cannot read and should not grow
// a dependency on for one benchmark input, so the conversion happens once
// out-of-band and this end stays an ifstream.
//
// Returns empty on any failure rather than throwing: a missing or unreadable
// file should cost the caller one dataset, not a whole sweep.
std::vector<s64> readInt64Column(const std::string& path, uint32_t max_count) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in.good()) {
    return {};
  }
  const auto bytes = static_cast<std::streamoff>(in.tellg());
  if (bytes <= 0) {
    return {};
  }

  // Short files are used as-is rather than cycled: repeating a column would
  // manufacture periodicity that flatters every codec measured on it.
  const auto available = static_cast<std::size_t>(bytes) / sizeof(s64);
  const auto count = std::min<std::size_t>(available, max_count);

  std::vector<s64> values(count);
  in.seekg(0);
  in.read(reinterpret_cast<char*>(values.data()),
          static_cast<std::streamsize>(count * sizeof(s64)));
  if (!in) {
    return {};
  }
  if (count < max_count) {
    std::cerr << "note: " << path << " holds " << available << " values, fewer than the requested "
              << max_count << "\n";
  }
  return values;
}
// -------------------------------------------------------------------------------------
std::vector<uint32_t> parseUintList(const std::string& text) {
  std::vector<uint32_t> values;
  std::stringstream stream(text);
  std::string token;
  while (std::getline(stream, token, ',')) {
    if (!token.empty()) {
      values.push_back(static_cast<uint32_t>(std::strtoul(token.c_str(), nullptr, 10)));
    }
  }
  return values;
}
// -------------------------------------------------------------------------------------
}  // namespace
// -------------------------------------------------------------------------------------
int main(int argc, char** argv) {
  uint32_t rows = 1u << 20;
  std::vector<uint32_t> block_sizes{4096, 8192, 65536};
  int repeats = 5;
  uint32_t seed = 42;
  std::string csv_path;
  std::string sections_csv_path;
  std::string input_i64;

  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    const auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
    if (arg == "--rows") {
      rows = static_cast<uint32_t>(std::strtoul(next().c_str(), nullptr, 10));
    } else if (arg == "--block-sizes") {
      block_sizes = parseUintList(next());
    } else if (arg == "--repeats") {
      repeats = std::atoi(next().c_str());
    } else if (arg == "--seed") {
      seed = static_cast<uint32_t>(std::strtoul(next().c_str(), nullptr, 10));
    } else if (arg == "--csv") {
      csv_path = next();
    } else if (arg == "--input-i64") {
      input_i64 = next();
    } else if (arg == "--sections-csv") {
      sections_csv_path = next();
    } else if (arg == "--help") {
      std::cout << "usage: subintsplit_bench [--rows N] [--block-sizes a,b,c] [--repeats N]\n"
                   "                         [--seed N] [--csv PATH] [--input-i64 PATH]\n"
                   "                         [--sections-csv PATH]\n"
                   "\n"
                   "  --input-i64 PATH     add a 64-bit dataset read from a flat little-endian\n"
                   "                       int64 file, for measuring against real data rather\n"
                   "                       than generated. Produce one with parquet_to_i64.py.\n"
                   "\n"
                   "  --sections-csv PATH  write one row per section of each SubIntSplit plan:\n"
                   "                       bit range, the scheme the planner predicted, the\n"
                   "                       scheme actually chosen, and the bytes it cost.\n";
      return 0;
    } else {
      std::cerr << "unknown argument: " << arg << "\n";
      return 1;
    }
  }

  BtrBlocksConfig::configure([](BtrBlocksConfig& config) {
    config.integers.schemes = defaultIntegerSchemes();
    config.integers.schemes.enable(IntegerSchemeType::SUB_INT_SPLIT);
    config.integers64.schemes = defaultInteger64Schemes();
    config.integers64.schemes.enable(Integer64SchemeType::SUB_INT_SPLIT);
    // FOR64 is a legacy scheme excluded from defaultInteger64Schemes() (like
    // its 32-bit counterpart), but the FOR64 codec below forces it directly,
    // so it has to be in the pool for the override to find.
    config.integers64.schemes.enable(Integer64SchemeType::FOR);
  });

  // The incumbent codecs, then SubIntSplit with the planner's split and with
  // the fixed halves split that isolates the planner's contribution.
  const std::vector<Codec> codecs32{
      {"UNCOMPRESSED", IntegerSchemeType::UNCOMPRESSED, false, ""},
      {"BP", IntegerSchemeType::BP, false, ""},
      {"PFOR", IntegerSchemeType::PFOR, false, ""},
      {"DICT", IntegerSchemeType::DICT, false, ""},
      {"RLE", IntegerSchemeType::RLE, false, ""},
      // What BtrBlocks does today, with SubIntSplit out of the pool.
      {"AUTO_BASELINE", IntegerSchemeType::UNCOMPRESSED, true, "", true},
      // And with it in, which also shows whether the picker actually selects it.
      {"AUTO_WITH_SIS", IntegerSchemeType::UNCOMPRESSED, true, ""},
      {"SIS_HALVES", IntegerSchemeType::SUB_INT_SPLIT, false, "0-15;16-31"},
      {"SIS_PLANNED", IntegerSchemeType::SUB_INT_SPLIT, false, ""},
  };
  // The incumbent 64-bit codecs (now real registered Integer64Schemes, not a
  // free-standing bolt-on -- see scheme/CompressionScheme64.hpp), then
  // SubIntSplit with the planner's split and with the fixed halves split.
  // UNCOMPRESSED64 is the baseline every other arm should beat on ratio;
  // BP64/FOR64/RLE64/DICT64 are what SIS64_PLANNED's point-access story
  // actually needs to be competitive with, matching the spirit of what
  // codecs32 compares SubIntSplit against.
  const std::vector<Codec64> codecs64{
      {"UNCOMPRESSED64", Integer64SchemeType::UNCOMPRESSED, false, ""},
      {"BP64", Integer64SchemeType::BP, false, ""},
      {"FOR64", Integer64SchemeType::FOR, false, ""},
      {"RLE64", Integer64SchemeType::RLE, false, ""},
      {"DICT64", Integer64SchemeType::DICT, false, ""},
      // What BtrBlocks does today, with SubIntSplit out of the pool.
      {"AUTO_BASELINE64", Integer64SchemeType::UNCOMPRESSED, true, "", true},
      // And with it in, which also shows whether the picker actually selects it.
      {"AUTO_WITH_SIS64", Integer64SchemeType::UNCOMPRESSED, true, ""},
      {"SIS64_HALVES", Integer64SchemeType::SUB_INT_SPLIT, false, "0-31;32-63"},
      {"SIS64_PLANNED", Integer64SchemeType::SUB_INT_SPLIT, false, ""},
  };

  struct Dataset32 {
    std::string name;
    std::vector<INTEGER> data;
  };
  struct Dataset64 {
    std::string name;
    std::vector<s64> data;
  };

  const std::vector<Dataset32> datasets32{
      {"snowflake", generateSnowflakes<INTEGER>(rows, instagramSnowflake32(), seed)},
      {"uniform", generateUniform<INTEGER>(rows, seed)},
      {"increasing", generateIncreasing<INTEGER>(rows, seed)},
  };
  std::vector<Dataset64> datasets64{
      {"snowflake", generateSnowflakes<s64>(rows, instagramSnowflake64(), seed)},
      {"uniform", generateUniform<s64>(rows, seed)},
      {"increasing", generateIncreasing<s64>(rows, seed)},
  };

  // Real identifiers, when a converted column is supplied. The generated
  // snowflake above is a controlled reference and an easy one -- its timestamp
  // advances monotonically, so every field has textbook structure. Real IDs
  // arrive unsorted, which removes exactly that structure, so this is the
  // number to quote.
  if (!input_i64.empty()) {
    auto values = readInt64Column(input_i64, rows);
    if (values.empty()) {
      std::cerr << "warning: could not read " << input_i64 << ", skipping the tweet_ids dataset\n";
    } else {
      datasets64.push_back({"tweet_ids", std::move(values)});
    }
  }

  std::ofstream file;
  if (!csv_path.empty()) {
    file.open(csv_path);
    if (!file.good()) {
      std::cerr << "cannot open " << csv_path << "\n";
      return 1;
    }
  }
  std::ostream& csv = csv_path.empty() ? std::cout : file;
  writeCsvHeader(csv);

  std::ofstream sections_file;
  if (!sections_csv_path.empty()) {
    sections_file.open(sections_csv_path);
    if (!sections_file.good()) {
      std::cerr << "cannot open " << sections_csv_path << "\n";
      return 1;
    }
    writeSectionsHeader(sections_file);
  }
  const auto emit = [&](const Result& result) {
    writeCsvRow(csv, result);
    if (sections_file.is_open()) {
      writeSectionRows(sections_file, result);
    }
  };

  for (const auto block_size : block_sizes) {
    for (const auto& dataset : datasets32) {
      for (const auto& codec : codecs32) {
        std::cerr << "32/" << dataset.name << "/" << codec.name << " @" << block_size << "\n";
        emit(run32(dataset.name, dataset.data, codec, block_size, repeats));
      }
    }
    for (const auto& dataset : datasets64) {
      for (const auto& codec : codecs64) {
        std::cerr << "64/" << dataset.name << "/" << codec.name << " @" << block_size << "\n";
        emit(run64(dataset.name, dataset.data, codec, block_size, repeats));
      }
    }
    csv.flush();
    if (sections_file.is_open()) {
      sections_file.flush();
    }
  }

  return 0;
}
