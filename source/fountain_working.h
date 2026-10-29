#pragma once

#include "source/fountain_intake.h"
#include <QStringList>

namespace apollo::source {
// Per-element content rules of the writer subset, shared by import admission
// and the canonical Fountain export contract (docs/fountain-export.md). Case
// follows docs/case-handling.md: a heading or cue may be authored in any ASCII
// case and is presented in capitals; uncapitalized_letter means presentation
// would still show a lowercase letter, such as é. The title must be authored in
// ASCII capitals (authored_case). spacing is the strict rule of writer-subset-1
// and -2: an empty line, or a leading, trailing or repeated space. Newer
// admission and the export refine it by element type (docs/spacing.md).
enum class WriterSubsetGap { character, authored_case, uncapitalized_letter, multiline, long_title, spacing };
std::vector<WriterSubsetGap> writer_subset_gaps(const Element& element);
// Conservative admission contracts; see docs/editable-fountain-subset.md. New
// copies use writer-subset-3, which admits the spacing in docs/spacing.md.
// Copies made under an earlier contract are validated by its rules:
// writer-subset-2 refused that spacing, and writer-subset-1 also required
// headings and cues in ASCII capitals. The original review model is
// immutable. Later writing may be unfinished.
inline constexpr auto writer_subset_contract = "writer-subset-3";
QStringList fountain_edit_blockers(const ProjectAssets& review, const QString& contract = writer_subset_contract);
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
