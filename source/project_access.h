#pragma once

#include "source/recovery.h"
#include <memory>

namespace apollo::source {
struct AccessCandidate {
  RecoveryCopy copy;
  QString origin_directory;
  Screenplay screenplay;
  ProjectAssets assets;
  QString relation;
};

// An explicit open decision, not an automatic rollback. Holds the original
// editable lock even when its manifest/source cannot be read. Disk operations
// belong on a worker; retaining this object keeps the decision locked.
class ProjectAccess {
public:
  explicit ProjectAccess(const QString& directory, const QString& expected_id = {});
  ~ProjectAccess();
  QString directory() const;
  QString error() const;
  bool current_available() const;
  QString project_id() const;
  std::uint64_t current_revision() const;
  const std::vector<AccessCandidate>& candidates() const;

  // Both operations revalidate the inspected decision and throw SourceError
  // on failure. A successful decision is consumed exactly once.
  std::unique_ptr<ProjectSession> open_current();
  // Forks the selected package into a new identity at an absolute, nonexistent
  // destination. Original/package evidence is never rewritten. If publication
  // succeeds but activation fails, the exception names the retained new folder.
  // before_activation runs after publication, for fault injection (as with
  // replace_file/save hooks); a throw retains and reports the published folder.
  std::unique_ptr<ProjectSession> recover(std::size_t index, const QString& destination,
                                          const std::function<void()>& before_activation = {});
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
