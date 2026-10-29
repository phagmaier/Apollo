#pragma once

#include "source/screenplay_format.h"

#include <QStringList>

namespace apollo::source {
// Contract fountain-canonical-2; see docs/fountain-export.md. These functions
// read only their arguments, so an export can run them on a worker thread.
inline constexpr auto fountain_export_contract = "fountain-canonical-2";
// The frozen revision-1 reader that verifies each export is bounded to 1 MiB.
inline constexpr qsizetype maximum_fountain_export_bytes = 1024 * 1024;

struct FountainExport {
  QByteArray bytes;     // Empty when blocked.
  QStringList blockers; // Why this snapshot cannot be exported, by element.
};
// The explicit, linear eligibility checks for a writing snapshot. Title-page
// fields are left out of both the checks and the output without include_title.
QStringList fountain_export_blockers(const std::vector<Element>& snapshot, bool include_title);
// The checks above, canonical serialization, then verification that the frozen
// revision-1 interpreter reads exactly this snapshot back without unresolved
// syntax or retained records. Bytes are returned only when all of that holds.
FountainExport export_fountain(const std::vector<Element>& snapshot, bool include_title);
// Scene headings and character cues are written as presented, in capitals
// (docs/case-handling.md). Says how many were authored otherwise, for the
// export disclosure; empty when none were.
QString fountain_case_note(const std::vector<Element>& snapshot);
}
