#pragma once

#include "source/screenplay_format.h"

#include <QString>
#include <functional>
#include <memory>
#include <optional>

class QLockFile;
class QFile;

namespace apollo::source {
inline constexpr int project_version = 1;

// A project folder: project.json names the project's persistent identity
// and its screenplay source file (docs/source-format.md).
struct Project {
  QString directory, id, screenplay_file;
  bool import_review = false;
  bool import_working = false;
  QString screenplay_path() const { return directory + QLatin1Char('/') + screenplay_file; }
};
// Immutable import dependencies, kept in memory so a copy can be complete
// even after the original destination becomes unavailable. Their import
// reader is separately versioned; copying preserves these bytes verbatim.
struct ProjectAssets {
  std::optional<QByteArray> imported_material, original_fountain;
  bool operator==(const ProjectAssets&) const = default;
};
struct OpenedProject { Project project; Screenplay screenplay; ProjectAssets assets; };

// Reads and validates the project and its screenplay without changing them.
// Throws SourceError.
OpenedProject open_project(const QString& directory);
// Writes a complete project in a staging folder beside destination, validates
// it, and only then gives it the destination name. Never replaces or reuses an
// existing destination, even an empty one. Elements without an id get one.
// Throws SourceError.
Project create_project(const QString& destination, Screenplay screenplay, const ProjectAssets& assets = {});

// Replacement steps, for fault injection and timing. A hook that throws makes
// the replacement fail at that step.
enum class WritePhase { opened, written, committed };
// Replaces path through a temporary file in its folder, so the old file stays
// complete until the new one is. Throws SourceError (io) naming the path.
void replace_file(const QString& path, const QByteArray& bytes, const std::function<void(WritePhase)>& hook = {});

// One editable session. Acquires a nonblocking, long-lived lock on the
// canonical folder before reading. Read-only open_project() needs no lock.
// After construction, use filesystem methods only on the ordered save worker.
// Only explicit accept_reload() adopts changed files as its new baseline.
class ProjectSession {
public:
  explicit ProjectSession(const QString& directory);
  ~ProjectSession();
  const OpenedProject& opened() const { return opened_; }
  void check_destination() const;
  void save(const QByteArray& bytes, const std::function<void(WritePhase)>& hook = {});
  // Reads a candidate while retaining the lock and the old save baseline.
  // Accept separately after the GUI confirms current work was preserved.
  OpenedProject read_reload() const;
  void accept_reload(const OpenedProject& candidate);
private:
  friend class ProjectAccess;
  // ProjectAccess retains this locked, unread session even when loading the
  // original source fails, so recovery cannot bypass a competing editor.
  struct InspectionOnly {};
  ProjectSession(const QString& directory, InspectionOnly);
  void load_current();
  void check_lock() const;
  struct Identity {
    quint64 device, inode;
    bool operator==(const Identity&) const = default;
  };
  struct FileState {
    Identity identity;
    QByteArray bytes;
    bool operator==(const FileState&) const = default;
  };
  static Identity identity(const QString& path, bool directory = false);
  static FileState read_file(const QString& path);
  OpenedProject opened_;
  Identity directory_identity_{};
  FileState manifest_, screenplay_, lock_file_;
  std::unique_ptr<QFile> directory_handle_;
  std::unique_ptr<QLockFile> lock_;
};
}
