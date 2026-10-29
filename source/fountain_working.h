#pragma once

#include "source/fountain_intake.h"
#include <QStringList>

namespace apollo::source {
// Per-element content rules of writer-subset-1, shared by import admission and
// the canonical Fountain export contract (docs/fountain-export.md).
enum class WriterSubsetGap { character, authored_case, multiline, long_title, spacing };
std::vector<WriterSubsetGap> writer_subset_gaps(const Element& element);
// Frozen, conservative admission contract; see docs/editable-fountain-subset.md.
// The original review model is immutable. Later writing may be unfinished.
QStringList fountain_edit_blockers(const ProjectAssets& review);
ProjectAssets editable_fountain_assets(const ProjectAssets& review);
bool is_fountain_working(const ProjectAssets& assets);
void validate_fountain_working(const ProjectAssets& assets);
Screenplay initial_fountain_writing(const ProjectAssets& assets);
FountainIntake open_working_fountain(const ProjectAssets& assets);
// Clean Fountain output is checked per snapshot (source/fountain_export.h);
// PDF needs separate layout evidence for imported writing.
inline const QString imported_pdf_block = QStringLiteral("Imported writing: PDF exports remain blocked pending independent layout validation of imported content. Fountain export checks the current writing separately.");
// Retained imported material that clean screenplay output leaves out, as a
// sentence for the export disclosure; empty for an ordinary project.
QString fountain_export_omissions(const ProjectAssets& assets);
// The absolute path an imported project was read from, or empty.
QString imported_origin(const ProjectAssets& assets);
}
