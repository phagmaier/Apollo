#pragma once

#include "engine/layout.h"

#include <QObject>
#include <QTextDocument>
#include <QVariantList>
#include <QVariantMap>

class QMimeData;

namespace apollo::editor {
class WritingSuggestions;
class Spelling;
class FindReplace;
// The spike's editing rules (docs/spike-brief.md, "Editor behavior under
// test"), applied to the document through QTextCursor. QML forwards the keys
// it intercepts. Each command is one undo step; commands that move the caret
// return its new position.
class EditorCommands : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList scenes READ scenes NOTIFY scenesChanged)
  Q_PROPERTY(QVariantList emptySpeeches READ emptySpeeches NOTIFY emptySpeechesChanged)
  Q_PROPERTY(QVariantList structureIssues READ structureIssues NOTIFY structureIssuesChanged)
  Q_PROPERTY(QString titleText READ titleText NOTIFY titleTextChanged)
  Q_PROPERTY(QString pageBreakMessage READ pageBreakMessage NOTIFY pageBreakMessageChanged)
  Q_PROPERTY(QString dualMessage READ dualMessage NOTIFY dualStateChanged)
  Q_PROPERTY(QString dualWarning READ dualWarning NOTIFY dualStateChanged)
  Q_PROPERTY(int documentRevision READ documentRevision NOTIFY documentStateChanged)
  Q_PROPERTY(QString typeMessage READ typeMessage NOTIFY typeMessageChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
  Q_PROPERTY(QObject* suggestions READ suggestions CONSTANT)
  Q_PROPERTY(QObject* spelling READ spelling CONSTANT)
  Q_PROPERTY(QObject* findReplace READ findReplace CONSTANT)
public:
  // Clipboard format that keeps element types, emphasis, and dual flags.
  static inline const QString mime_type = QStringLiteral("application/x-apollo-elements");

  explicit EditorCommands(QTextDocument* document, QObject* parent = nullptr);
  // Typing is grouped by word: a new undo step starts with each word, after
  // the caret moves, or after any other change to the document.
  Q_INVOKABLE int typeText(int position, int selectionStart, int selectionEnd, const QString& text);
  Q_INVOKABLE int enter(int position, int selectionStart, int selectionEnd);
  Q_INVOKABLE void cycleType(int position, bool backward);
  // Body type names from the source format. A local selection changes its
  // whole element; spanning elements or changing protected structure refuses.
  Q_INVOKABLE QString elementTypeAt(int position) const;
  Q_INVOKABLE void setElementType(int position, int selectionStart, int selectionEnd, const QString& type);
  int documentRevision() const { return document_->revision(); }
  bool canUndo() const { return document_->isUndoAvailable(); }
  bool canRedo() const { return document_->isRedoAvailable(); }
  QObject* suggestions() const;
  QObject* spelling() const;
  QObject* findReplace() const;
  const QString& typeMessage() const { return type_message_; }
  // Single caret in the second speech; changes only its cue flag, undoably.
  Q_INVOKABLE void toggleDualDialogue(int position, int selectionStart, int selectionEnd);
  const QString& dualMessage() const { return dual_message_; }
  const QString& dualWarning() const { return dual_warning_; }
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
  // Live headings: stable id, presented text and current UTF-16 position.
  const QVariantList& scenes() const { return scenes_; }
  Q_INVOKABLE QString sceneAt(int position) const;
  Q_INVOKABLE int scenePosition(const QString& id) const;
  // Move before another heading, or to the end with an empty beforeId.
  // A stale revision, missing id or unchanged order refuses without editing.
  // Both selection endpoints follow their original element and offset.
  Q_INVOKABLE QVariantMap moveScene(const QString& id, const QString& beforeId, int revision, int position, int anchor);
  // Cached diagnostics, refreshed when the document changes. Locations are
  // current UTF-16 positions; ids survive edits and undo/redo.
  // Speech-structure issues in document order (docs/diagnostics.md): {key,
  // kind, id, position, end, text, explanation, corrections: [{action, label,
  // enabled}]}. Kinds: emptySpeech, speechWithoutCue, adjacentDialogue and
  // incompleteDual. emptySpeeches is the emptySpeech subset.
  const QVariantList& structureIssues() const { return structure_issues_; }
  // {start, end} of the issue's current elements; empty once it is resolved.
  Q_INVOKABLE QVariantMap locateStructureIssue(const QString& key) const;
  // Applies one offered correction in one undo step and returns the caret
  // position, or -1 without editing when the issue/correction is stale.
  Q_INVOKABLE int correctStructureIssue(const QString& key, const QString& action);
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
  void scenesChanged();
  void emptySpeechesChanged();
  void structureIssuesChanged();
  void titleTextChanged();
  void pageBreakMessageChanged();
  void dualStateChanged();
  void documentStateChanged();
  void typeMessageChanged();
  void historyChanged();
  void textCommitted(int position, int revision);

private:
  QTextDocument* document_;
  WritingSuggestions* suggestions_;
  Spelling* spelling_;
  FindReplace* find_replace_;
  int typed_end_ = -1, typed_steps_ = -1;
  QVariantList empty_speeches_, structure_issues_, scenes_;
  QString title_text_;
  QString page_break_message_;
  QString dual_message_, dual_warning_;
  QString type_message_;
  bool forwarding_input_ = false;
  void setPageBreakMessage(const QString& message);
  bool refuseMarkerSelection(int start, int end);
  bool refuseDualSelection(int start, int end);
  bool refuseTitleSelection(int start, int end);
  int pasteData(int position, int selectionStart, int selectionEnd, const QMimeData* data);
  void setDualMessage(const QString& message);
  void setTypeMessage(const QString& message);
  void moveToWriting(QTextCursor& cursor, bool skipTitle = false);
  void refreshDocumentState();
};
}
