#pragma once

#include "source/project.h"
#include <QVariantList>

namespace apollo::source {
// Metadata v1 is unconverted intake with candidate hints. Explicit v2 copies
// carry versioned located interpretations from fountain_conversion.h. Both are
// review-only; neither enables the writer or clean screenplay export.
struct FountainIntake {
  ProjectAssets assets;
  QString summary, source_view;
  QVariantList findings;
  bool interpreted = false;
};
inline constexpr qsizetype maximum_fountain_bytes = 16 * 1024 * 1024;
inline constexpr qsizetype maximum_fountain_metadata_bytes = 64 * 1024 * 1024;
FountainIntake read_fountain_intake(const QString& path);
// Strictly validates the typed metadata, original length and SHA-256 before
// constructing the reader. Invalid/newer evidence is never rewritten.
FountainIntake open_fountain_intake(const ProjectAssets& assets);
void validate_fountain_intake(const ProjectAssets& assets);
// Recognizes the reserved format; full validation is done by open above.
bool is_fountain_intake(const ProjectAssets& assets);
}
