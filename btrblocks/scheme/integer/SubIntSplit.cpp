// -------------------------------------------------------------------------------------
#include "SubIntSplit.hpp"
// -------------------------------------------------------------------------------------
#include "common/Exceptions.hpp"
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
// Placeholder implementation. The scheme is registered so that its enum value,
// name and configuration are in place, but it reports a compression ratio of 0
// and is absent from defaultIntegerSchemes(), so the picker can never select
// it. Encoding and decoding land in a follow-up commit.
// -------------------------------------------------------------------------------------
double SubIntSplit::expectedCompressionRatio(SInteger32Stats&, u8) {
  return 0;
}
// -------------------------------------------------------------------------------------
u32 SubIntSplit::compress(const INTEGER*, const BITMAP*, u8*, SInteger32Stats&, u8) {
  throw Generic_Exception("SubIntSplit compression is not implemented yet");
}
// -------------------------------------------------------------------------------------
void SubIntSplit::decompress(INTEGER*, BitmapWrapper*, const u8*, u32, u32) {
  throw Generic_Exception("SubIntSplit decompression is not implemented yet");
}
// -------------------------------------------------------------------------------------
std::string SubIntSplit::fullDescription(const u8*) {
  return this->selfDescription();
}
// -------------------------------------------------------------------------------------
INTEGER SubIntSplit::lookup(u32) {
  UNREACHABLE();
}
void SubIntSplit::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
