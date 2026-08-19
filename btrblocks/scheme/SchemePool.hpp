#pragma once
#include "scheme/CompressionScheme.hpp"
#include "scheme/CompressionScheme64.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
struct SchemesCollection {
  std::unordered_map<IntegerSchemeType, unique_ptr<IntegerScheme>> integer_schemes;
  std::unordered_map<Integer64SchemeType, unique_ptr<Integer64Scheme>> integer64_schemes;
  std::unordered_map<DoubleSchemeType, unique_ptr<DoubleScheme>> double_schemes;
  std::unordered_map<StringSchemeType, unique_ptr<StringScheme>> string_schemes;
  SchemesCollection();
};
// -------------------------------------------------------------------------------------
class SchemePool {
 public:
  static unique_ptr<SchemesCollection> available_schemes;
  // -------------------------------------------------------------------------------------
  static void refresh();
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
