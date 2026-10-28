#pragma once

#include <QString>
#include <vector>

namespace apollo::editor {
struct RecentProject {
  QString directory;
  QString project_id;
};

class ProjectLibrary {
public:
  // Empty selects GenericDataLocation/Apollo/recent-projects.json, never source.
  explicit ProjectLibrary(QString path = {});
  // Missing lists are empty. Invalid/newer/nonregular lists throw SourceError
  // naming the list path, without changing the original evidence.
  std::vector<RecentProject> load() const;
  // Only call after successful activation of this actual project identity.
  // Canonicalize, promote, deduplicate by identity OR directory, and keep 20.
  // A busy read-modify-write lock fails immediately with SourceError::locked.
  void record(const RecentProject& project) const;

private:
  QString path_;
};
}
