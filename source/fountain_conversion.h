#pragma once

#include "source/project.h"
#include <QJsonObject>

namespace apollo::source {
// Zero-based, half-open offsets into the immutable original bytes, including
// BOM and CRLF. Line numbers are one-based and include the final occupied byte.
struct FountainRange {
  qsizetype start = 0, end = 0;
  int first_line = 1, last_line = 1;
  bool operator==(const FountainRange&) const = default;
};
struct LocatedFountainElement {
  Element element;
  FountainRange range;
  // Exact contributing source slices, in text order, before emphasis decoding.
  // Gaps can contain removed notes, delimiters, keys or indentation.
  std::vector<FountainRange> text_sources;
};
struct FountainRecord {
  QString id, kind, name, text, key, anchor, parent;
  int depth = 0;
  FountainRange range;
};
struct FountainIssue { QString code, message; FountainRange range; };
struct FountainConversion {
  std::vector<LocatedFountainElement> elements;
  std::vector<FountainRecord> records;
  std::vector<FountainIssue> unresolved;
  // Rendering/authoring gaps are distinct from unresolved source syntax.
  std::vector<FountainIssue> limitations;
  Screenplay screenplay() const;
  QJsonObject json() const;
};

// Validates its intake. No filesystem writes and no use of the spike
// importer. Revision 1 is frozen for reopening versioned interpretation copies.
FountainConversion convert_fountain(const ProjectAssets& assets);
// The same frozen revision-1 interpreter for text that is not an intake, such
// as Apollo's own canonical export when verifying that it reads back.
FountainConversion interpret_fountain(const QByteArray& bytes);
// Makes v2 intake metadata with a complete, deterministic interpretation. The
// original byte array, origin, length and digest are carried forward unchanged.
// Callers publish through staged create_project, never update a review in place.
ProjectAssets interpreted_fountain_assets(const ProjectAssets& assets);
// Internal cross-file validation step: called after intake evidence validation.
void validate_fountain_conversion(const ProjectAssets& assets);
}
