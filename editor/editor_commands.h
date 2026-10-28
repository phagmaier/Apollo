#pragma once

#include "engine/layout.h"

#include <QObject>
#include <QTextDocument>

namespace apollo::editor {
// The spike's editing rules (docs/spike-brief.md, "Editor behavior under
// test"), applied to the document through QTextCursor. QML forwards the keys
// it intercepts. Each command is one undo step; commands that move the caret
// return its new position.
class EditorCommands : public QObject {
  Q_OBJECT
public:
  // Clipboard format that keeps element types, emphasis, and dual flags.
  static inline const QString mime_type = QStringLiteral("application/x-apollo-elements");

  explicit EditorCommands(QTextDocument* document, QObject* parent = nullptr);
  // Typing is grouped by word: a new undo step starts with each word, after
  // the caret moves, or after any other change to the document.
  Q_INVOKABLE int typeText(int position, int selectionStart, int selectionEnd, const QString& text);
  Q_INVOKABLE int enter(int position, int selectionStart, int selectionEnd);
  Q_INVOKABLE void cycleType(int position, bool backward);
  Q_INVOKABLE void copy(int selectionStart, int selectionEnd);
  Q_INVOKABLE int cut(int selectionStart, int selectionEnd);
  Q_INVOKABLE int paste(int position, int selectionStart, int selectionEnd);
  // Moves the scene containing position above the previous scene. Returns
  // the caret's new position, or -1 when there is no previous scene.
  Q_INVOKABLE int moveSceneUp(int position);

private:
  QTextDocument* document_;
  int typed_end_ = -1, typed_steps_ = -1;
};
}
