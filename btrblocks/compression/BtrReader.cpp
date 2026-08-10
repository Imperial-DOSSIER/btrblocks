#include "BtrReader.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <vector>
#include "common/Exceptions.hpp"
#include "compression/SchemePicker.hpp"
#include "extern/RoaringBitmap.hpp"

namespace btrblocks {

BtrReader::BtrReader(void* data) : data(data) {
  this->m_bitmap_wrappers = std::vector<BitmapWrapper*>(this->getChunkCount(), nullptr);
  this->m_bitsets = std::vector<boost::dynamic_bitset<>*>(this->getChunkCount(), nullptr);
}

BtrReader::~BtrReader() {
  // bitsets are deleted by BitmapWrapper deconstructor
  // (unless we release the bitmap first in which case the wrapper is deleted,
  // but the bitset may still exist)
  for (std::size_t i = 0; i < this->m_bitmap_wrappers.size(); i++) {
    BitmapWrapper* wrapper = this->m_bitmap_wrappers[i];
    if (wrapper == nullptr) {
      delete this->m_bitsets[i];
    } else {
      delete wrapper;
    }
  }
}

bool BtrReader::readColumn(std::vector<u8>& output_chunk_v, u32 index) {
  // Fetch metadata for column
  auto meta = this->getChunkMetadata(index);

  // Get a pointer to the beginning of the memory area with the data
  auto input_data = static_cast<const u8*>(meta->data);

  // Decompress bitmap
  u32 tuple_count = meta->tuple_count;
  BitmapWrapper* bitmap = this->getBitmap(index);

  auto output_chunk = get_data(output_chunk_v, this->getDecompressedSize(index) + SIMD_EXTRA_BYTES);
  bool requires_copy = false;
  // Decompress data
  switch (meta->type) {
    case ColumnType::INTEGER: {
      // Prepare destination array
      auto destination_array = reinterpret_cast<INTEGER*>(output_chunk);

      // Fetch the scheme from metadata
      auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(meta->compression_type);
      scheme.decompress(destination_array, bitmap, input_data, tuple_count, 0);
      break;
    }
    case ColumnType::DOUBLE: {
      // Prepare destination array
      auto destination_array = reinterpret_cast<DOUBLE*>(output_chunk);

      auto& scheme = DoubleSchemePicker::MyTypeWrapper::getScheme(meta->compression_type);
      scheme.decompress(destination_array, bitmap, input_data, tuple_count, 0);
      break;
    }
    case ColumnType::STRING: {
      auto& scheme = StringSchemePicker::MyTypeWrapper::getScheme(meta->compression_type);
      requires_copy = scheme.decompressNoCopy(output_chunk, bitmap, input_data, tuple_count, 0);
      break;
    }
    default: {
      throw Generic_Exception("Type " + ConvertTypeToString(meta->type) + " not supported");
    }
  }

  return requires_copy;
}

void BtrReader::gatherColumn(INTEGER* dest,
                             const u32* positions,
                             u32 position_count,
                             u32* chunks_touched) {
  if (chunks_touched != nullptr) {
    *chunks_touched = 0;
  }
  if (position_count == 0) {
    return;
  }
  if (this->getColumnType() != ColumnType::INTEGER) {
    throw Generic_Exception("gatherColumn is only supported for INTEGER columns");
  }

  const u32 chunk_count = this->getChunkCount();

  // Prefix sum of per-chunk tuple counts. Derived from metadata rather than
  // assuming a uniform block_size, because the final chunk is short.
  std::vector<u32> chunk_start(chunk_count + 1, 0);
  for (u32 chunk_i = 0; chunk_i < chunk_count; chunk_i++) {
    chunk_start[chunk_i + 1] = chunk_start[chunk_i] + this->getTupleCount(chunk_i);
  }
  const u32 total_tuples = chunk_start[chunk_count];

  // Bucket the requested positions by chunk, remembering where each one came
  // from so results can be scattered back in the caller's order.
  std::vector<std::vector<u32>> local_positions(chunk_count);
  std::vector<std::vector<u32>> output_slots(chunk_count);
  for (u32 i = 0; i < position_count; i++) {
    const u32 position = positions[i];
    if (position >= total_tuples) {
      throw Generic_Exception("gatherColumn position out of range");
    }
    // chunk_start is sorted; find the chunk containing this row.
    const auto it = std::upper_bound(chunk_start.begin(), chunk_start.end(), position);
    const auto chunk_i = static_cast<u32>(std::distance(chunk_start.begin(), it) - 1);
    local_positions[chunk_i].push_back(position - chunk_start[chunk_i]);
    output_slots[chunk_i].push_back(i);
  }

  std::vector<INTEGER> chunk_results;
  u32 touched = 0;
  for (u32 chunk_i = 0; chunk_i < chunk_count; chunk_i++) {
    if (local_positions[chunk_i].empty()) {
      continue;
    }
    touched++;

    auto meta = this->getChunkMetadata(chunk_i);
    auto input_data = static_cast<const u8*>(meta->data);
    BitmapWrapper* bitmap = this->getBitmap(chunk_i);
    auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(meta->compression_type);

    const auto count = static_cast<u32>(local_positions[chunk_i].size());
    chunk_results.resize(count);
    scheme.gather(chunk_results.data(), input_data, bitmap, meta->tuple_count,
                  local_positions[chunk_i].data(), count, 0);

    for (u32 j = 0; j < count; j++) {
      dest[output_slots[chunk_i][j]] = chunk_results[j];
    }
  }

  if (chunks_touched != nullptr) {
    *chunks_touched = touched;
  }
}
// -------------------------------------------------------------------------------------
INTEGER BtrReader::lookupColumn(u32 position) {
  INTEGER result = 0;
  this->gatherColumn(&result, &position, 1);
  return result;
}
// -------------------------------------------------------------------------------------
string BtrReader::getSchemeDescription(u32 index) {
  auto meta = this->getChunkMetadata(index);
  u8 compression = meta->compression_type;
  auto src = static_cast<const u8*>(meta->data);

  switch (meta->type) {
    case ColumnType::INTEGER: {
      auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(compression);
      return scheme.fullDescription(src);
    }
    case ColumnType::DOUBLE: {
      auto& scheme = DoubleSchemePicker::MyTypeWrapper::getScheme(compression);
      return scheme.fullDescription(src);
    }
    case ColumnType::STRING: {
      auto& scheme = StringSchemePicker::MyTypeWrapper::getScheme(compression);
      return scheme.fullDescription(src);
    }
    default:
      throw Generic_Exception("Type " + ConvertTypeToString(meta->type) + " not supported");
  }
}

string BtrReader::getBasicSchemeDescription(u32 index) {
  // Only print the first level of the scheme description instead of all of them
  auto meta = this->getChunkMetadata(index);
  u8 compression = meta->compression_type;
  auto src = static_cast<const u8*>(meta->data);

  switch (meta->type) {
    case ColumnType::INTEGER: {
      auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(compression);
      return scheme.selfDescription();
    }
    case ColumnType::DOUBLE: {
      auto& scheme = DoubleSchemePicker::MyTypeWrapper::getScheme(compression);
      return scheme.selfDescription();
    }
    case ColumnType::STRING: {
      auto& scheme = StringSchemePicker::MyTypeWrapper::getScheme(compression);
      return scheme.selfDescription(src);
    }
    default:
      throw Generic_Exception("Type " + ConvertTypeToString(meta->type) + " not supported");
  }
}

// TODO make the bitset thread local
BitmapWrapper* BtrReader::getBitmap(u32 index) {
  if (this->m_bitmap_wrappers[index] != nullptr) {
    return this->m_bitmap_wrappers[index];
  }

  auto meta = this->getChunkMetadata(index);
  auto type = meta->nullmap_type;
  // Allocate bitset if it's not yet there
  if (this->m_bitsets[index] == nullptr && type != BitmapType::ALLONES &&
      type != BitmapType::ALLZEROS) {
    this->m_bitsets[index] = new boost::dynamic_bitset<>(meta->tuple_count);
  }
  // TODO if there are too many page fault try to do the allocation of the
  // bitset inside the object beforehand
  this->m_bitmap_wrappers[index] = new BitmapWrapper(meta->data + meta->nullmap_offset, type,
                                                     meta->tuple_count, this->m_bitsets[index]);
  return this->m_bitmap_wrappers[index];
}

void BtrReader::releaseBitmap(u32 index) {
  if (this->m_bitmap_wrappers[index] == nullptr) {
    return;
  }
  this->m_bitmap_wrappers[index]->releaseBitset();
  delete this->m_bitmap_wrappers[index];
  this->m_bitmap_wrappers[index] = nullptr;
}

BitmapWrapper* BtrReader::releaseBitmapOwnership(u32 index) {
  BitmapWrapper* ret = this->m_bitmap_wrappers[index];
  this->m_bitmap_wrappers[index] = nullptr;
  this->m_bitsets[index] = nullptr;
  return ret;
}

u32 BtrReader::getDecompressedSize(u32 index) {
  auto meta = this->getChunkMetadata(index);

  switch (meta->type) {
    case ColumnType::INTEGER: {
      return sizeof(INTEGER) * meta->tuple_count;
    }
    case ColumnType::DOUBLE: {
      return sizeof(DOUBLE) * meta->tuple_count;
    }
    case ColumnType::STRING: {
      auto& scheme = StringSchemePicker::MyTypeWrapper::getScheme(meta->compression_type);

      auto input_data = static_cast<const u8*>(meta->data);
      BitmapWrapper* bitmapWrapper = this->getBitmap(index);
      u32 size = scheme.getDecompressedSizeNoCopy(input_data, meta->tuple_count, bitmapWrapper);
      // TODO The 4096 is temporary until I figure out why FSST is returning
      // bigger numbers
      return size + 8 + 4096;  // +8 because of fsst decompression
    }
    default: {
      throw Generic_Exception("Type " + ConvertTypeToString(this->getColumnType()) +
                              " not supported");
    }
  }
}

u32 BtrReader::getDecompressedDataSize(u32 index) {
  auto meta = this->getChunkMetadata(index);
  switch (meta->type) {
    case ColumnType::INTEGER: {
      return sizeof(INTEGER) * meta->tuple_count;
    }
    case ColumnType::DOUBLE: {
      return sizeof(DOUBLE) * meta->tuple_count;
    }
    case ColumnType::STRING: {
      auto& scheme = StringSchemePicker::MyTypeWrapper::getScheme(meta->compression_type);

      auto input_data = static_cast<const u8*>(meta->data);
      BitmapWrapper* bitmapWrapper = this->getBitmap(index);
      u32 size = scheme.getTotalLength(input_data, meta->tuple_count, bitmapWrapper);
      return size;
    }
    default: {
      throw Generic_Exception("Type " + ConvertTypeToString(meta->type) + " not supported");
    }
  }
}
}  // namespace btrblocks
