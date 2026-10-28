#pragma once

#include <QString>
#include <optional>

class QTextDocument;

namespace apollo::editor {
struct ResumeState {
  QString directory;
  QString project_id;
  QString element_id;
  // Both coordinates count Qt UTF-16 units, not source code points/UTF-8 bytes.
  int offset = 0;
  int position = 0;
};

class SessionState {
public:
  // Empty selects GenericDataLocation/Apollo/session.json, outside the project.
  explicit SessionState(QString path = {});
  // A missing file has no state. Invalid, newer, or unreadable state throws
  // source::SourceError naming the state path; reading never changes evidence.
  std::optional<ResumeState> load() const;
  // Atomic replacement. Refuses to overwrite malformed/newer existing state.
  void save(const ResumeState& state) const;

  static ResumeState capture(const QString& directory, const QString& project_id,
                             const QTextDocument& document, int cursor);
  // Prefer element identity; otherwise use the absolute position. Clamp to
  // available text and retreat from the middle of a UTF-16 surrogate pair.
  static int cursor_position(const ResumeState& state, const QTextDocument& document);

private:
  QString path_;
};
}
