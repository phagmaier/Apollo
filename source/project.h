#pragma once

#include "source/screenplay_format.h"

#include <QString>
#include <functional>

namespace apollo::source {
inline constexpr int project_version = 1;

// A project folder: project.json names the project's persistent identity
// and its screenplay source file (docs/source-format.md).
struct Project {
  QString directory, id, screenplay_file;
  QString screenplay_path() const { return directory + QLatin1Char('/') + screenplay_file; }
};
struct OpenedProject { Project project; Screenplay screenplay; };

// Reads and validates the project and its screenplay without changing them.
// Throws SourceError.
OpenedProject open_project(const QString& directory);
// Writes a complete project in a staging folder beside destination, validates
// it, and only then gives it the destination name. Never replaces or reuses an
// existing destination, even an empty one. Elements without an id get one.
// Throws SourceError.
Project create_project(const QString& destination, Screenplay screenplay);

// Replacement steps, for fault injection and timing. A hook that throws makes
// the replacement fail at that step.
enum class WritePhase { opened, written, committed };
// Replaces path through a temporary file in its folder, so the old file stays
// complete until the new one is. Throws SourceError (io) naming the path.
void replace_file(const QString& path, const QByteArray& bytes, const std::function<void(WritePhase)>& hook = {});
}
