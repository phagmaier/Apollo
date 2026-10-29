#pragma once

#include "engine/layout.h"

#include <QObject>
#include <QTextDocument>
#include <QVariantList>

namespace apollo::editor {
// The spike's editing rules (docs/spike-brief.md, "Editor behavior under
// test"), applied to the document through QTextCursor. QML forwards the keys
// it intercepts. Each command is one undo step; commands that move the caret
// return its new position.
class EditorCommands : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList emptySpeeches READ emptySpeeches NOTIFY emptySpeechesChanged)
  Q_PROPERTY(QString titleText READ titleText NOTIFY titleTextChanged)
  Q_PROPERTY(QString pageBreakMessage READ pageBreakMessage NOTIFY pageBreakMessageChanged)
public:
  // Clipboard format that keeps element types, emphasis, and dual flags.
  static inline const QString mime_type = QStringLiteral("application/x-apollo-elements");

  explicit EditorCommands(QTextDocument* document, QObject* parent = nullptr);
  // Typing is grouped by word: a new undo step starts with each word, after
  // the caret moves, or after any other change to the document.
  Q_INVOKABLE int typeText(int position, int selectionStart, int selectionEnd, const QString& text);
  Q_INVOKABLE int enter(int position, int selectionStart, int selectionEnd);
  Q_INVOKABLE void cycleType(int position, bool backward);
  // Selection only; a mixed selection gains the style, full coverage removes
  // it. Names are bold, italic, underline. Breaks are not styled.
  Q_INVOKABLE void toggleEmphasis(int selectionStart, int selectionEnd, const QString& style);
  Q_INVOKABLE void copy(int selectionStart, int selectionEnd);
  Q_INVOKABLE int cut(int selectionStart, int selectionEnd);
  Q_INVOKABLE int paste(int position, int selectionStart, int selectionEnd);
  // Markers never replace selected writing or split an element/speech.
  Q_INVOKABLE int insertPageBreak(int position, int selectionStart, int selectionEnd);
  Q_INVOKABLE int removePageBreak(int position, int selectionStart, int selectionEnd);
  // -1 delegates an ordinary deletion to TextEdit; otherwise handled/refused.
  Q_INVOKABLE int deleteText(int position, int selectionStart, int selectionEnd, bool backward);
  Q_INVOKABLE bool isPageBreak(int position) const;
  Q_INVOKABLE int editablePosition(int position, bool backward = false) const;
  const QString& pageBreakMessage() const { return page_break_message_; }
  bool eventFilter(QObject* watched, QEvent* event) override;
  // Moves the scene containing position above the previous scene. Returns
  // the caret's new position, or -1 when there is no previous scene.
  Q_INVOKABLE int moveSceneUp(int position);
  // Cached diagnostics, refreshed when the document changes. Locations are
  // current UTF-16 positions; ids survive edits and undo/redo.
  const QVariantList& emptySpeeches() const { return empty_speeches_; }
  Q_INVOKABLE int locateEmptySpeech(const QString& id) const;
  // One undoable type-only change. Refuses stale, valid, or dual-related speech.
  Q_INVOKABLE int treatSpeechAsAction(const QString& id);
  // Live title entries in document order: {id, field, text}. Existing ids
  // must keep their type and relative order; an empty id creates an entry.
  Q_INVOKABLE QVariantList titlePage() const;
  // Full desired list, including empty entries. Omission explicitly removes
  // an existing title. One undo step; invalid or unchanged lists do not edit.
  Q_INVOKABLE bool setTitlePage(const QVariantList& entries);
  const QString& titleText() const { return title_text_; }

signals:
  void emptySpeechesChanged();
  void titleTextChanged();
  void pageBreakMessageChanged();

private:
  QTextDocument* document_;
  int typed_end_ = -1, typed_steps_ = -1;
  QVariantList empty_speeches_;
  QString title_text_;
  QString page_break_message_;
  bool forwarding_input_ = false;
  void setPageBreakMessage(const QString& message);
  bool refuseMarkerSelection(int start, int end);
  void moveToWriting(QTextCursor& cursor, bool skipTitle = false);
  void refreshDocumentState();
};
}
