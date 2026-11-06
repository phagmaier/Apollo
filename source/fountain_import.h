#pragma once

#include "source/project.h"

namespace apollo::source {
// Simple Fountain import (docs/fountain-import.md). The Fountain file is only
// read, never written. Import converts it into an ordinary Apollo screenplay
// and creates a new ordinary project; there is no review step, no retained
// original and no admission gate. Material Apollo has no place for yet (notes,
// boneyard, sections, synopses, scene numbers, other title keys) is left out
// and summarized in `notice`; `notice` is empty when everything carried over.
struct FountainImport {
  Screenplay screenplay;
  QString notice;
};
// Throws SourceError when the text cannot be read as Fountain (not UTF-8, or
// beyond the converter's 1 MiB / 100,000 line bound).
FountainImport import_fountain_text(const QByteArray& bytes);

struct ImportedProject {
  Project project;
  QString notice;
};
// Reads fountain_path, converts it and publishes a new project named `name`
// inside `parent` with create_named_project's naming rules. Throws SourceError.
ImportedProject import_fountain_project(const QString& parent, const QString& name, const QString& fountain_path);
// The same at an exact new destination folder (command line and tests).
ImportedProject import_fountain_project_at(const QString& destination, const QString& fountain_path);
}
