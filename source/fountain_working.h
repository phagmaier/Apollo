#pragma once

#include "source/fountain_intake.h"
#include <QStringList>

namespace apollo::source {
// Frozen, conservative admission contract; see docs/editable-fountain-subset.md.
// The original review model is immutable. Later writing may be unfinished.
QStringList fountain_edit_blockers(const ProjectAssets& review);
ProjectAssets editable_fountain_assets(const ProjectAssets& review);
bool is_fountain_working(const ProjectAssets& assets);
void validate_fountain_working(const ProjectAssets& assets);
Screenplay initial_fountain_writing(const ProjectAssets& assets);
FountainIntake open_working_fountain(const ProjectAssets& assets);
inline const QString fountain_export_block = QStringLiteral("Imported writing: clean Fountain and PDF exports remain blocked pending independent Fountain round-trip and complete layout validation.");
}
