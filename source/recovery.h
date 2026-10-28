#pragma once

#include "source/project.h"
#include <QDateTime>

namespace apollo::source {
struct RecoveryCopy {
  QString directory, reason;
  QDateTime captured_at;
  std::uint64_t revision;
};

// A fully validated package, including provenance and immutable dependencies.
// Inspection never repairs, rewrites, or removes an unrecognized package.
struct RecoveryPackage {
  RecoveryCopy copy;
  QString origin_id, origin_directory;
  OpenedProject opened;
};

// Complete immutable project packages. Invalid packages are left as evidence,
// excluded from offers and never pruned. Call on the ordered save worker.
class RecoveryStore {
public:
  RecoveryStore(Project origin, ProjectAssets assets, QString directory);
  static QString emergency_directory(const Project& origin);
  static std::optional<RecoveryPackage> inspect_copy(const QString& directory);
  RecoveryCopy preserve(const Screenplay& screenplay, const QString& reason,
                        const QDateTime& captured_at = QDateTime::currentDateTimeUtc());
  std::vector<RecoveryCopy> verified() const;
private:
  std::optional<RecoveryCopy> verified_copy(const QString& path) const;
  Project origin_;
  ProjectAssets assets_;
  QString directory_;
};
}
