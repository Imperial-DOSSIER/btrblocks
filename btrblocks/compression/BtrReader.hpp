//
// Created by david on 18.04.22.
//

#pragma once

#include <filesystem>
#include "compression/Datablock.hpp"

namespace btrblocks {

class BtrReader {
 public:
  explicit BtrReader(void* data);
  virtual ~BtrReader();
  bool readColumn(std::vector<u8>& output_chunk, u32 index);
  // -------------------------------------------------------------------------------------
  // Column-level random access. `positions` are global row indices across the
  // whole part; `dest` receives one value per requested position, in the order
  // given. Positions are bucketed by chunk, so each touched chunk is decoded at
  // most once regardless of how many positions fall inside it.
  //
  // Codec-agnostic: dispatches per chunk on that chunk's own compression_type,
  // exactly as readColumn does, so it works for every registered scheme and
  // picks up any scheme's native gather() override for free.
  //
  // INTEGER columns only; throws for other column types.
  //
  // `chunks_touched`, when non-null, receives the number of distinct chunks
  // that had to be decoded -- the dominant term in the cost of a gather, and
  // needed to interpret gather timings.
  // -------------------------------------------------------------------------------------
  void gatherColumn(INTEGER* dest,
                    const u32* positions,
                    u32 position_count,
                    u32* chunks_touched = nullptr);
  [[nodiscard]] INTEGER lookupColumn(u32 position);
  // -------------------------------------------------------------------------------------
  [[nodiscard]] string getSchemeDescription(u32 index);
  [[nodiscard]] string getBasicSchemeDescription(u32 index);

  [[nodiscard]] u32 getDecompressedSize(u32 index);
  // Only the whole decompressed data without the btr specific metadata around
  // it.
  [[nodiscard]] u32 getDecompressedDataSize(u32 index);
  [[nodiscard]] BitmapWrapper* getBitmap(u32 index);
  void releaseBitmap(u32 index);
  BitmapWrapper* releaseBitmapOwnership(u32 index);

  [[nodiscard]] inline const ColumnPartMetadata* getPartMetadata() {
    return reinterpret_cast<const ColumnPartMetadata*>(this->data);
  }
  [[nodiscard]] inline const ColumnChunkMeta* getChunkMetadata(u32 index) {
    u32 offset = this->getPartMetadata()->offsets[index];
    return reinterpret_cast<const ColumnChunkMeta*>(reinterpret_cast<char*>(this->data) + offset);
  }
  [[nodiscard]] inline u32 getTupleCount(u32 index) {
    return this->getChunkMetadata(index)->tuple_count;
  }
  [[nodiscard]] inline ColumnType getColumnType() { return this->getChunkMetadata(0)->type; }
  [[nodiscard]] inline u32 getChunkCount() { return this->getPartMetadata()->num_chunks; }

 private:
  void* data{};
  std::vector<BitmapWrapper*> m_bitmap_wrappers;
  std::vector<boost::dynamic_bitset<>*> m_bitsets;
};

}  // namespace btrblocks
