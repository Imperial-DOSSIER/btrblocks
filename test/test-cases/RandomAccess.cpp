// -------------------------------------------------------------------------------------
// Tests for the random-access API added to IntegerScheme and BtrReader.
//
// These are deliberately parametrised over *every registered integer scheme*
// rather than over one scheme of interest: the contract being tested is that
// gather()/lookupAt() agree with decompress()-then-index for any scheme, which
// is what makes the API usable as a fair baseline across codecs.
// -------------------------------------------------------------------------------------
#include "TestHelper.hpp"
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "common/SIMD.hpp"
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "compression/Datablock.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/SchemePool.hpp"
#include "storage/Chunk.hpp"
#include "storage/Relation.hpp"
// -------------------------------------------------------------------------------------
#include "gtest/gtest.h"
// -------------------------------------------------------------------------------------
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
namespace {
// -------------------------------------------------------------------------------------
// Values that every integer scheme can compress without tripping over its
// preconditions: a narrow non-negative range (safe for Truncation8/16 and the
// fixed dictionaries), low cardinality (DICT), and runs (RLE).
std::vector<INTEGER> makeUniversallyCompressibleData(u32 count, u32 seed = 42) {
  std::mt19937 gen(seed);
  std::vector<INTEGER> data;
  data.reserve(count);
  while (data.size() < count) {
    const auto value = static_cast<INTEGER>(gen() % 200);
    const u32 run = 1 + (gen() % 8);
    for (u32 i = 0; i < run && data.size() < count; i++) {
      data.push_back(value);
    }
  }
  return data;
}
// -------------------------------------------------------------------------------------
std::vector<u32> makeRandomPositions(u32 count, u32 upper_bound, u32 seed = 7) {
  std::mt19937 gen(seed);
  std::vector<u32> positions(count);
  for (auto& position : positions) {
    position = gen() % upper_bound;
  }
  return positions;
}
// -------------------------------------------------------------------------------------
// Runs the gather/lookupAt contract against one scheme on one dataset.
void checkGatherAgainstDecompress(IntegerScheme& scheme,
                                  const std::vector<INTEGER>& data,
                                  const std::string& scheme_name) {
  const auto tuple_count = static_cast<u32>(data.size());
  const std::vector<BITMAP> bitmap(tuple_count, 1);

  auto stats = SInteger32Stats::generateStats(data.data(), bitmap.data(), tuple_count);
  auto compressed = makeBytesArray(tuple_count * sizeof(INTEGER) * 10 + 4096);
  ASSERT_NO_THROW(scheme.compress(data.data(), bitmap.data(), compressed.get(), stats, 3))
      << "scheme " << scheme_name;

  BitmapWrapper nullmap(nullptr, BitmapType::ALLONES, tuple_count);

  // Reference: whole-chunk decompress. The SIMD slack is required because
  // TRLE stores whole vectors past the logical end of the buffer.
  std::vector<INTEGER> reference(tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER));
  scheme.decompress(reference.data(), &nullmap, compressed.get(), tuple_count, 0);

  const auto positions = makeRandomPositions(std::min<u32>(tuple_count, 512), tuple_count);
  std::vector<INTEGER> gathered(positions.size());
  scheme.gather(gathered.data(), compressed.get(), &nullmap, tuple_count, positions.data(),
                static_cast<u32>(positions.size()), 0);

  for (std::size_t i = 0; i < positions.size(); i++) {
    ASSERT_EQ(gathered[i], reference[positions[i]])
        << "scheme " << scheme_name << ", gather index " << i << " (row " << positions[i] << ")";
  }

  // lookupAt must agree with gather on a handful of single rows.
  for (u32 i = 0; i < std::min<u32>(tuple_count, 16); i++) {
    const u32 position = positions[i];
    ASSERT_EQ(scheme.lookupAt(compressed.get(), &nullmap, tuple_count, position, 0),
              reference[position])
        << "scheme " << scheme_name << ", lookupAt row " << position;
  }
}
// -------------------------------------------------------------------------------------
}  // namespace
// -------------------------------------------------------------------------------------
TEST(RandomAccess, Begin) {
  // Exercise every scheme that can actually round-trip, which is every
  // registered scheme except TRUNCATION_8/16.
  //
  // Those two are write-only in this codebase: ITruncCompress is implemented
  // and ITruncExpectedCF reports a positive ratio whenever the value range fits
  // the code type, but ITruncDecompress (Truncation.hpp:97) is a bare
  // UNREACHABLE() -- i.e. __builtin_unreachable(), undefined behaviour rather
  // than a trap. Enabling them lets the picker cascade into a stream that can
  // never be read back, which corrupts the stack on decompress. Pre-existing,
  // and orthogonal to the random-access API under test here.
  auto schemes = IntegerSchemeSet{};
  schemes.enableAll();
  schemes.disable(IntegerSchemeType::TRUNCATION_8);
  schemes.disable(IntegerSchemeType::TRUNCATION_16);
  BtrBlocksConfig::get().integers.schemes = schemes;
  BtrBlocksConfig::get().doubles.schemes = defaultDoubleSchemes();
  BtrBlocksConfig::get().strings.schemes = defaultStringSchemes();
  SchemePool::refresh();
}
// -------------------------------------------------------------------------------------
// The headline test: the default gather()/lookupAt() implementations must be
// correct for every scheme, with no per-scheme work. This is also what catches
// a scratch buffer sized without SIMD slack, since TRLE overruns it.
TEST(RandomAccess, GatherMatchesDecompressForEveryScheme) {
  const auto data = makeUniversallyCompressibleData(9000);
  ASSERT_FALSE(SchemePool::available_schemes->integer_schemes.empty());

  for (auto& entry : SchemePool::available_schemes->integer_schemes) {
    const auto scheme_name = ConvertSchemeTypeToString(entry.first);
    SCOPED_TRACE("scheme = " + scheme_name);
    checkGatherAgainstDecompress(*entry.second, data, scheme_name);
  }
}
// -------------------------------------------------------------------------------------
TEST(RandomAccess, GatherMatchesDecompressOnConstantData) {
  const std::vector<INTEGER> data(4096, 1234);

  for (auto& entry : SchemePool::available_schemes->integer_schemes) {
    const auto scheme_name = ConvertSchemeTypeToString(entry.first);
    SCOPED_TRACE("scheme = " + scheme_name);
    checkGatherAgainstDecompress(*entry.second, data, scheme_name);
  }
}
// -------------------------------------------------------------------------------------
TEST(RandomAccess, GatherHandlesEmptyRequests) {
  const auto data = makeUniversallyCompressibleData(1024);
  const auto tuple_count = static_cast<u32>(data.size());
  const std::vector<BITMAP> bitmap(tuple_count, 1);

  auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(CB(IntegerSchemeType::UNCOMPRESSED));
  auto stats = SInteger32Stats::generateStats(data.data(), bitmap.data(), tuple_count);
  auto compressed = makeBytesArray(tuple_count * sizeof(INTEGER) * 10 + 4096);
  scheme.compress(data.data(), bitmap.data(), compressed.get(), stats, 3);

  BitmapWrapper nullmap(nullptr, BitmapType::ALLONES, tuple_count);

  // Zero positions and zero tuples must both be no-ops rather than crashes.
  INTEGER sentinel = -1;
  scheme.gather(&sentinel, compressed.get(), &nullmap, tuple_count, nullptr, 0, 0);
  ASSERT_EQ(sentinel, -1);

  const u32 position = 0;
  scheme.gather(&sentinel, compressed.get(), &nullmap, 0, &position, 1, 0);
  ASSERT_EQ(sentinel, -1);
}
// -------------------------------------------------------------------------------------
// Column-level gather spans chunks, and chunks may use different schemes.
// block_size is forced small so the column is genuinely multi-chunk.
TEST(RandomAccess, GatherColumnMatchesReadColumn) {
  const u32 saved_block_size = BtrBlocksConfig::get().block_size;
  BtrBlocksConfig::get().block_size = 8192;

  const u32 tuple_count = 50000;
  const auto data = makeUniversallyCompressibleData(tuple_count, 99);

  Relation relation;
  {
    Vector<INTEGER> column_data(tuple_count);
    for (u32 i = 0; i < tuple_count; i++) {
      column_data[i] = data[i];
    }
    relation.addColumn({"ids", std::move(column_data)});
  }

  const auto ranges = relation.getRanges(SplitStrategy::SEQUENTIAL, 9999);
  ASSERT_GT(ranges.size(), 1u) << "test needs a multi-chunk column";

  ColumnPart part;
  for (std::size_t chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
    auto input_chunk = relation.getInputChunk(ranges[chunk_i], chunk_i, 0);
    auto compressed = Datablock::compress(input_chunk);
    part.addCompressedChunk(std::move(compressed));
  }

  const std::string path = "gather_column_test.btr";
  part.writeToDisk(path);

  std::vector<char> file_contents;
  Utils::readFileToMemory(path, file_contents);
  BtrReader reader(file_contents.data());

  ASSERT_GT(reader.getChunkCount(), 1u);

  // Reference: decode every chunk and concatenate.
  std::vector<INTEGER> reference;
  reference.reserve(tuple_count);
  for (u32 chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
    std::vector<u8> output;
    reader.readColumn(output, chunk_i);
    const auto* values = reinterpret_cast<const INTEGER*>(output.data());
    for (u32 i = 0; i < reader.getTupleCount(chunk_i); i++) {
      reference.push_back(values[i]);
    }
  }
  ASSERT_EQ(reference.size(), tuple_count);

  const auto positions = makeRandomPositions(4096, tuple_count, 3);
  std::vector<INTEGER> gathered(positions.size());
  u32 chunks_touched = 0;
  reader.gatherColumn(gathered.data(), positions.data(), static_cast<u32>(positions.size()),
                      &chunks_touched);

  for (std::size_t i = 0; i < positions.size(); i++) {
    ASSERT_EQ(gathered[i], reference[positions[i]]) << "row " << positions[i];
  }
  // With this many uniform positions every chunk is touched -- the effect that
  // makes uniform gather degenerate into a full column decode.
  ASSERT_EQ(chunks_touched, reader.getChunkCount());

  // A single lookup must touch exactly one chunk.
  ASSERT_EQ(reader.lookupColumn(12345), reference[12345]);

  const u32 single = 40000;
  INTEGER single_result = 0;
  reader.gatherColumn(&single_result, &single, 1, &chunks_touched);
  ASSERT_EQ(single_result, reference[single]);
  ASSERT_EQ(chunks_touched, 1u);

  std::remove(path.c_str());
  BtrBlocksConfig::get().block_size = saved_block_size;
}
// -------------------------------------------------------------------------------------
TEST(RandomAccess, End) {
  BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
  SchemePool::refresh();
}
// -------------------------------------------------------------------------------------
