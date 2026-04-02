#ifndef QMDINPUTMETADATA_HH
#define QMDINPUTMETADATA_HH

namespace qmd_input_metadata {

enum class SourceKind : int {
  kUnknown = 0,
  kElastic = 1,
  kAllevent = 2
};

enum class PolarizationKind : int {
  kUnknown = 0,
  kY = 1,
  kZ = 2
};

constexpr int kSourceKindIndex = 0;
constexpr int kOriginalEventIdIndex = 1;
constexpr int kSourceFileIndex = 2;
constexpr int kPolarizationKindIndex = 3;

constexpr int kBimpIndex = 0;

}  // namespace qmd_input_metadata

#endif
