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
// Current character repertoire; punctuation is preserved, never transliterated.
bool writer_subset_character(char32_t codepoint);
std::vector<WriterSubsetGap> writer_subset_gaps(const Element& element, bool typographic_punctuation = true);
// Conservative admission contracts; see docs/editable-fountain-subset.md.
// Revision 5 preserves unfinished empty speeches for explicit correction and
// permits a separate opt-in to normal scene spacing, retaining original gaps.
// Earlier copies are always validated under their original stricter rules.
inline constexpr auto writer_subset_contract = "writer-subset-5";
inline constexpr qsizetype writer_subset_maximum_bytes = 128 * 1024;
inline constexpr std::size_t writer_subset_maximum_elements = 3072;
int fountain_scene_spacing_count(const ProjectAssets& review);
QStringList fountain_edit_blockers(const ProjectAssets& review, const QString& contract = writer_subset_contract,
                                  bool normalize_scene_spacing = false);
ProjectAssets editable_fountain_assets(const ProjectAssets& review, bool normalize_scene_spacing = false);
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
