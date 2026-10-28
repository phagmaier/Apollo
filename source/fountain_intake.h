#pragma once

#include "source/project.h"
#include <QVariantList>

namespace apollo::source {
// This is intake, not a screenplay parser. Every byte remains pending conversion.
// Findings are review hints, never evidence of complete syntax recognition.
struct FountainIntake {
  ProjectAssets assets;
  QString summary, source_view;
  QVariantList findings;
};
inline constexpr qsizetype maximum_fountain_bytes = 16 * 1024 * 1024;
FountainIntake read_fountain_intake(const QString& path);
// Strictly validates the typed metadata, original length and SHA-256 before
// constructing the reader. Invalid/newer evidence is never rewritten.
FountainIntake open_fountain_intake(const ProjectAssets& assets);
void validate_fountain_intake(const ProjectAssets& assets);
// Recognizes the reserved format; full validation is done by open above.
bool is_fountain_intake(const ProjectAssets& assets);
}
