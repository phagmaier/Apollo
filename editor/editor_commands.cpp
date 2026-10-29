#include "editor/editor_commands.h"
#include "editor/screenplay_layout.h"
#include "source/screenplay_format.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QMouseEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QRegularExpression>
#include <QSet>
#include <QScopedValueRollback>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <algorithm>
#include <array>
#include <optional>

namespace apollo::editor {
namespace {
// Tab order from the brief; Shift+Tab runs it backward.
constexpr std::array cycle{ElementType::heading, ElementType::action, ElementType::cue,
                           ElementType::parenthetical, ElementType::dialogue, ElementType::transition};

std::size_t utf8_offset(const QString& text, int utf16) {
  return element_text(text.left(utf16)).size();
}

QTextCursor selection(QTextDocument* document, int position, int start, int end) {
  QTextCursor cursor(document);
  if (start != end) { cursor.setPosition(start); cursor.setPosition(end, QTextCursor::KeepAnchor); }
  else cursor.setPosition(position);
  return cursor;
}

QString new_id() { return QString::fromStdString(source::new_element_id()); }

constexpr std::array title_types{ElementType::title, ElementType::credit, ElementType::author,
                                 ElementType::source, ElementType::contact, ElementType::draft_date};
const std::array title_fields{QStringLiteral("title"), QStringLiteral("credit"), QStringLiteral("author"),
                              QStringLiteral("source"), QStringLiteral("contact"), QStringLiteral("draftDate")};

int title_index(ElementType type) {
  const auto found = std::find(title_types.begin(), title_types.end(), type);
  return found == title_types.end() ? -1 : static_cast<int>(found - title_types.begin());
}

QTextBlock block_with_id(QTextDocument* document, const QString& id) {
  for (auto block = document->begin(); block.isValid(); block = block.next())
    if (block.blockFormat().stringProperty(element_id_property) == id) return block;
  return {};
}

// Keep the identical prefix and suffix in place, including their character
// formats. Only the changed middle is plain text. Never split a surrogate pair.
void replace_title_text(QTextCursor& cursor, const QTextBlock& block, const QString& text) {
  const auto old = block.text();
  qsizetype prefix = 0;
  while (prefix < old.size() && prefix < text.size() && old[prefix] == text[prefix]) ++prefix;
  if (prefix && old[prefix - 1].isHighSurrogate()) --prefix;
  qsizetype suffix = 0;
  while (suffix < old.size() - prefix && suffix < text.size() - prefix &&
         old[old.size() - suffix - 1] == text[text.size() - suffix - 1]) ++suffix;
  if (suffix && old[old.size() - suffix].isLowSurrogate()) --suffix;
  cursor.setPosition(block.position() + static_cast<int>(prefix));
  cursor.setPosition(block.position() + static_cast<int>(old.size() - suffix), QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  cursor.insertText(text.mid(prefix, text.size() - prefix - suffix), QTextCharFormat());
}

void remove_element_block(QTextCursor& cursor, const QTextBlock& block) {
  if (const auto next = block.next(); next.isValid()) {
    const auto format = next.blockFormat();
    const auto characters = next.charFormat();
    cursor.setPosition(block.position());
    cursor.setPosition(next.position(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    // Removing a paragraph at its start retains that paragraph's format;
    // the surviving next element must retain its own identity and type.
    cursor.setBlockFormat(format);
    cursor.setBlockCharFormat(characters);
  } else if (const auto previous = block.previous(); previous.isValid()) {
    cursor.setPosition(previous.position() + previous.length() - 1);
    cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
  } else {
    cursor.select(QTextCursor::BlockUnderCursor);
    cursor.removeSelectedText();
    QTextBlockFormat format;
    format.setProperty(element_type_property, static_cast<int>(ElementType::action));
    format.setProperty(element_id_property, new_id());
    cursor.setBlockFormat(format);
    cursor.setBlockCharFormat(QTextCharFormat());
  }
}

void set_type(QTextCursor& cursor, ElementType type) {
  QTextBlockFormat format;
  format.setProperty(element_type_property, static_cast<int>(type));
  cursor.mergeBlockFormat(format);
}

// A parenthetical belongs to a speech when a cue leads its run of speech parts.
bool in_speech(QTextBlock block) {
  for (block = block.previous(); block.isValid(); block = block.previous()) {
    const auto type = element_type(block);
    if (type == ElementType::cue) return true;
    if (type != ElementType::dialogue && type != ElementType::parenthetical) return false;
  }
  return false;
}

struct SpeechState {
  QTextBlock end;
  bool has_dialogue = false;
  bool dual_related = false;
};

SpeechState speech_state(QTextBlock cue) {
  SpeechState state{cue.next(), false, cue.blockFormat().boolProperty(dual_right_property)};
  while (state.end.isValid()) {
    const auto type = element_type(state.end);
    if (type != ElementType::parenthetical && type != ElementType::dialogue) break;
    state.has_dialogue |= type == ElementType::dialogue;
    state.dual_related |= state.end.blockFormat().boolProperty(dual_right_property);
    state.end = state.end.next();
  }
  // A following right-hand cue depends on this cue as its left-hand speech.
  if (state.end.isValid() && element_type(state.end) == ElementType::cue)
    state.dual_related |= state.end.blockFormat().boolProperty(dual_right_property);
  return state;
}

bool speech_part(ElementType type) {
  return type == ElementType::dialogue || type == ElementType::parenthetical;
}

// A speech and its adjacent dual partner are indivisible for break placement,
// including unfinished speeches. Never insert between a cue and its parts.
std::pair<QTextBlock, QTextBlock> writing_group(QTextBlock block) {
  auto first = block, last = block;
  while (speech_part(element_type(first)) && first.previous().isValid() &&
         (speech_part(element_type(first.previous())) || element_type(first.previous()) == ElementType::cue))
    first = first.previous();
  if (element_type(first) != ElementType::cue) {
    while (last.next().isValid() && speech_part(element_type(last)) && speech_part(element_type(last.next())))
      last = last.next();
    return {first, last};
  }
  while (first.blockFormat().boolProperty(dual_right_property)) {
    auto previous = first.previous();
    while (previous.isValid() && speech_part(element_type(previous))) previous = previous.previous();
    if (!previous.isValid() || element_type(previous) != ElementType::cue) break;
    first = previous;
  }
  last = first;
  for (;;) {
    while (last.next().isValid() && speech_part(element_type(last.next()))) last = last.next();
    const auto next = last.next();
    if (!next.isValid() || element_type(next) != ElementType::cue ||
        !next.blockFormat().boolProperty(dual_right_property)) break;
    last = next;
  }
  return {first, last};
}

QTextBlock speech_cue(QTextBlock block) {
  while (block.isValid() && speech_part(element_type(block))) block = block.previous();
  return block.isValid() && element_type(block) == ElementType::cue ? block : QTextBlock{};
}

bool complete_speech(QTextBlock cue) {
  if (!cue.isValid() || cue.text().trimmed().isEmpty()) return false;
  for (auto block = cue.next(); block.isValid() && speech_part(element_type(block)); block = block.next())
    if (element_type(block) == ElementType::dialogue && !block.text().trimmed().isEmpty()) return true;
  return false;
}

bool dual_member(QTextBlock block) {
  if (!block.isValid() || !speech_cue(block).isValid()) return false;
  const auto [first, last] = writing_group(block);
  for (auto current = first; current.isValid(); current = current.next()) {
    if (current.blockFormat().boolProperty(dual_right_property)) return true;
    if (current == last) break;
  }
  return false;
}

const auto unpair_first = QStringLiteral("Turn Dual Dialogue off in the second speech before changing the pair's structure. Text and emphasis can still be edited.");

bool paragraph_break(const QString& text) {
  return text.contains('\n') || text.contains('\r') || text.contains(QChar::ParagraphSeparator);
}

QTextBlockFormat element_format(ElementType type) {
  QTextBlockFormat format;
  format.setProperty(element_type_property, static_cast<int>(type));
  format.setProperty(element_id_property, new_id());
  return format;
}

// Qt gives the original block to the new empty prefix. Put its original
// format on the surviving text, and a new identity on the inserted marker.
void insert_before(QTextCursor& cursor, const QTextBlock& block, ElementType type) {
  const auto format = block.blockFormat();
  const auto characters = block.charFormat();
  const int at = block.position();
  cursor.setPosition(at);
  cursor.insertBlock(format, characters);
  cursor.setPosition(at);
  cursor.setBlockFormat(element_format(type));
  cursor.setBlockCharFormat(QTextCharFormat());
}

QTextBlock selected_marker(QTextDocument* document, int start, int end) {
  const auto [from, to] = std::minmax(start, end);
  if (to - from != 1) return {};
  const auto before = document->findBlock(from), after = document->findBlock(to);
  if (before.isValid() && element_type(before) == ElementType::page_break && before.text().isEmpty()) return before;
  if (after.isValid() && element_type(after) == ElementType::page_break && after.text().isEmpty() &&
      from == before.position() + before.length() - 1) return after;
  return {};
}

int nearby_writing(const QTextBlock& block, bool backward) {
  auto find = [&](bool previous) {
    auto adjacent = previous ? block.previous() : block.next();
    while (adjacent.isValid() && (element_type(adjacent) == ElementType::page_break ||
                                  title_index(element_type(adjacent)) >= 0))
      adjacent = previous ? adjacent.previous() : adjacent.next();
    return adjacent.isValid() ? adjacent.position() + (previous ? adjacent.length() - 1 : 0) : -1;
  };
  const int preferred = find(backward);
  return preferred < 0 ? find(!backward) : preferred;
}

QTextBlock cue_with_id(QTextDocument* document, const QString& id) {
  if (id.isEmpty()) return {};
  for (auto block = document->begin(); block.isValid(); block = block.next())
    if (block.blockFormat().stringProperty(element_id_property) == id)
      return element_type(block) == ElementType::cue ? block : QTextBlock{};
  return {};
}

// The elements covering [start, end), each cut to the selected part. A
// selection edge that covers none of a block's text leaves that block out.
std::vector<Element> selected_elements(QTextDocument* document, int start, int end) {
  if (const auto marker = selected_marker(document, start, end); marker.isValid())
    return {block_element(marker)};
  std::vector<Element> elements;
  const auto first = document->findBlock(start), last = document->findBlock(end);
  for (auto block = first; block.isValid(); block = block.next()) {
    const auto text = block.text();
    const int from = std::max(start - block.position(), 0), to = std::min(end - block.position(), static_cast<int>(text.size()));
    const bool edge_only = first != last && ((block == first && from == text.size()) || (block == last && to == 0));
    if (!edge_only || (element_type(block) == ElementType::page_break && block.position() < end))
      elements.push_back(split_element(split_element(block_element(block), utf8_offset(text, to)).first, utf8_offset(text, from)).second);
    if (block == last) break;
  }
  return elements;
}

QByteArray serialize(const std::vector<Element>& elements) {
  QJsonArray array;
  for (const auto& element : elements) {
    QJsonArray styles;
    for (const auto& span : element.styles)
      styles.append(QJsonArray{static_cast<qint64>(span.start), static_cast<qint64>(span.end), static_cast<int>(span.style)});
    array.append(QJsonObject{{"type", static_cast<int>(element.type)}, {"text", QString::fromStdString(element.text)},
                             {"dual", element.dual_right}, {"styles", styles}});
  }
  return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

std::optional<std::vector<Element>> deserialize(const QByteArray& data) {
  const auto json = QJsonDocument::fromJson(data);
  if (!json.isArray() || json.array().isEmpty()) return std::nullopt;
  std::vector<Element> elements;
  for (const auto& value : json.array()) {
    if (!value.isObject()) return std::nullopt;
    const auto object = value.toObject();
    const int type = object["type"].toInt(-1);
    if (type < 0 || type > static_cast<int>(ElementType::draft_date) || !object["text"].isString() ||
        !object["dual"].isBool() || !object["styles"].isArray()) return std::nullopt;
    const auto text = object["text"].toString();
    // Apollo's source '\n' is an internal break, not an element boundary.
    if (!text.isValidUtf16() || text.contains(QRegularExpression(QStringLiteral("[\\r\\t\\x{2028}\\x{2029}\\x{fdd0}\\x{fdd1}]"))))
      return std::nullopt;
    Element element{static_cast<ElementType>(type), text.toStdString(), object["dual"].toBool()};
    auto boundary = [&](std::size_t at) {
      return at == element.text.size() || (static_cast<unsigned char>(element.text[at]) & 0xc0) != 0x80;
    };
    for (const auto& span : object["styles"].toArray()) {
      const auto parts = span.toArray();
      if (parts.size() != 3) return std::nullopt;
      const auto start = parts[0].toInteger(-1), end = parts[1].toInteger(-1);
      const int style = parts[2].toInt(-1);
      if (start < 0 || end <= start || static_cast<quint64>(end) > element.text.size() ||
          style < 0 || style > static_cast<int>(Style::underline)) return std::nullopt;
      const auto from = static_cast<std::size_t>(start), to = static_cast<std::size_t>(end);
      if (!boundary(from) || !boundary(to)) return std::nullopt;
      element.styles.push_back({from, to, static_cast<Style>(style)});
    }
    elements.push_back(std::move(element));
  }
  return elements;
}

// Splits the cursor's element at the cursor and puts elements between the
// two halves, dropping a half that is empty. Returns the position after the
// last inserted element.
int insert_between(QTextCursor& cursor, const std::vector<Element>& inserted) {
  const auto block = cursor.block();
  const auto split = split_element(block_element(block), utf8_offset(block.text(), cursor.positionInBlock()));
  std::vector<Element> elements;
  if (!split.first.text.empty()) elements.push_back(split.first);
  elements.insert(elements.end(), inserted.begin(), inserted.end());
  int caret = block.position() - 1;
  for (const auto& element : elements) caret += static_cast<int>(QString::fromStdString(element.text).size()) + 1;
  if (!split.second.text.empty()) elements.push_back(split.second);
  cursor.setPosition(block.position());
  cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  insert_elements(cursor, elements);
  return caret;
}
}

EditorCommands::EditorCommands(QTextDocument* document, QObject* parent) : QObject(parent), document_(document) {
  connect(document_, &QTextDocument::contentsChanged, this, &EditorCommands::refreshDocumentState);
  refreshDocumentState();
}

void EditorCommands::refreshDocumentState() {
  QVariantList issues;
  QString title, dual_warning;
  for (auto block = document_->begin(); block.isValid();) {
    if (element_type(block) == ElementType::title)
      title = QString(block.text()).replace(QChar::LineSeparator, QLatin1Char(' '));
    if (element_type(block) != ElementType::cue) { block = block.next(); continue; }
    const auto state = speech_state(block);
    if (block.blockFormat().boolProperty(dual_right_property)) {
      const auto left = speech_cue(block.previous());
      if (!left.isValid() || left.blockFormat().boolProperty(dual_right_property) ||
          !complete_speech(left) || !complete_speech(block))
        dual_warning = QStringLiteral("Incomplete or invalid Dual Dialogue: the pairing is retained. Complete both adjacent speeches, or turn Dual Dialogue off in the second speech. Fountain export may be refused; incomplete layout is a preview.");
    }
    if (!state.has_dialogue) {
      issues.push_back(QVariantMap{
          {"id", block.blockFormat().stringProperty(element_id_property)},
          {"position", block.position()}, {"text", block.text()},
          {"canTreatAsAction", !state.dual_related},
          {"explanation", state.dual_related
              ? QStringLiteral("This speech has a dual-dialogue relationship. Treating it as action could change its pairing; edit the speech manually.")
              : QStringLiteral("This cue has no dialogue. Add dialogue, or explicitly treat the cue and its parentheticals as action.")}});
    }
    block = state.end;
  }
  if (dual_warning != dual_warning_) {
    dual_warning_ = dual_warning;
    emit dualStateChanged();
  }
  if (title != title_text_) {
    title_text_ = std::move(title);
    emit titleTextChanged();
  }
  if (issues == empty_speeches_) return;
  empty_speeches_ = std::move(issues);
  emit emptySpeechesChanged();
}

void EditorCommands::setDualMessage(const QString& message) {
  if (dual_message_ == message) return;
  dual_message_ = message;
  emit dualStateChanged();
}

bool EditorCommands::refuseDualSelection(int start, int end) {
  const auto [from, to] = std::minmax(start, end);
  if (document_->findBlock(from) == document_->findBlock(to)) return false;
  for (auto block = document_->findBlock(from); block.isValid() && block.position() <= to; block = block.next()) {
    if (dual_member(block)) { setDualMessage(unpair_first); return true; }
  }
  return false;
}

void EditorCommands::toggleDualDialogue(int position, int start, int end) {
  setPageBreakMessage({});
  if (start != end) { setDualMessage(QStringLiteral("Place a single caret in the second speech to change Dual Dialogue; selected writing is kept.")); return; }
  const auto cue = speech_cue(document_->findBlock(position));
  if (!cue.isValid()) { setDualMessage(QStringLiteral("Place the caret in the second character cue, parenthetical or dialogue to change Dual Dialogue.")); return; }
  const bool paired = cue.blockFormat().boolProperty(dual_right_property);
  const auto following = speech_state(cue).end;
  if (paired && following.isValid() && element_type(following) == ElementType::cue &&
      following.blockFormat().boolProperty(dual_right_property)) {
    setDualMessage(QStringLiteral("This invalid chain has another dual cue after it. Turn Dual Dialogue off in the last speech first, so no partner is silently reassigned.")); return;
  }
  if (!paired) {
    const auto previous = speech_cue(cue.previous());
    if (!previous.isValid() || !complete_speech(previous) || !complete_speech(cue)) {
      setDualMessage(QStringLiteral("Dual Dialogue needs two complete, immediately adjacent speeches, each with a nonblank cue and dialogue. It cannot cross action, scenes or page breaks.")); return;
    }
    if (dual_member(previous) || dual_member(cue)) {
      setDualMessage(QStringLiteral("A speech is already paired. Place the caret in that pair's second speech and turn Dual Dialogue off first.")); return;
    }
  }
  QTextCursor cursor(cue);
  cursor.beginEditBlock();
  auto format = cue.blockFormat();
  if (paired) format.clearProperty(dual_right_property);
  else format.setProperty(dual_right_property, true);
  cursor.setBlockFormat(format);
  cursor.endEditBlock();
  typed_end_ = typed_steps_ = -1;
  setDualMessage({});
}

int EditorCommands::locateEmptySpeech(const QString& id) const {
  const auto cue = cue_with_id(document_, id);
  return cue.isValid() && !speech_state(cue).has_dialogue ? cue.position() : -1;
}

int EditorCommands::treatSpeechAsAction(const QString& id) {
  const auto cue = cue_with_id(document_, id);
  if (!cue.isValid()) return -1;
  const auto state = speech_state(cue);
  if (state.has_dialogue || state.dual_related) return -1;
  const int position = cue.position();
  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  for (auto block = cue; block != state.end; block = block.next()) {
    cursor.setPosition(block.position());
    set_type(cursor, ElementType::action);
  }
  cursor.endEditBlock();
  return position;
}

QVariantList EditorCommands::titlePage() const {
  QVariantList entries;
  for (auto block = document_->begin(); block.isValid(); block = block.next()) {
    const int index = title_index(element_type(block));
    if (index < 0) continue;
    entries.append(QVariantMap{{"id", block.blockFormat().stringProperty(element_id_property)},
                              {"field", title_fields[index]},
                              {"text", QString(block.text()).replace(QChar::LineSeparator, QLatin1Char('\n'))}});
  }
  return entries;
}

bool EditorCommands::setTitlePage(const QVariantList& entries) {
  struct Entry { QString id; ElementType type; QString text; };
  std::vector<Entry> desired;
  desired.reserve(static_cast<std::size_t>(entries.size()));
  QSet<QString> retained;
  int last_existing = -1;
  bool changed = false;
  // Validate the complete submission against the live document before opening
  // an edit block. No string coercion, stale references, or type retargeting.
  for (const auto& value : entries) {
    if (value.metaType() != QMetaType::fromType<QVariantMap>()) return false;
    const auto entry = value.toMap();
    for (const auto* key : {"id", "field", "text"})
      if (entry.value(key).metaType() != QMetaType::fromType<QString>()) return false;
    const auto id = entry.value("id").toString();
    const auto field = entry.value("field").toString();
    auto text = entry.value("text").toString();
    const auto found = std::find(title_fields.begin(), title_fields.end(), field);
    if (found == title_fields.end() || !text.isValidUtf16()) return false;
    for (const auto character : text)
      if (character == QLatin1Char('\r') || character == QChar::LineSeparator ||
          character == QChar::ParagraphSeparator || character == QChar(0xfdd0) || character == QChar(0xfdd1))
        return false;
    const auto type = title_types[static_cast<std::size_t>(found - title_fields.begin())];
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    if (!id.isEmpty()) {
      const auto block = block_with_id(document_, id);
      if (retained.contains(id) || !block.isValid() || element_type(block) != type ||
          block.blockNumber() <= last_existing) return false;
      last_existing = block.blockNumber();
      retained.insert(id);
      changed |= block.text() != text;
    } else changed = true;
    desired.push_back({id, type, std::move(text)});
  }
  QStringList removed;
  for (auto block = document_->begin(); block.isValid(); block = block.next()) {
    if (title_index(element_type(block)) < 0) continue;
    const auto id = block.blockFormat().stringProperty(element_id_property);
    if (!retained.contains(id)) removed.append(id);
  }
  if (!changed && removed.isEmpty()) return true;

  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  typed_end_ = typed_steps_ = -1;
  for (std::size_t i = 0; i < desired.size(); ++i) {
    const auto& entry = desired[i];
    if (!entry.id.isEmpty()) {
      const auto block = block_with_id(document_, entry.id);
      if (block.text() != entry.text) replace_title_text(cursor, block, entry.text);
      continue;
    }
    // Insert before the next retained title, or the first body element,
    // whichever comes first. With no body or following title, append.
    QTextBlock before;
    for (auto block = document_->begin(); block.isValid(); block = block.next()) {
      if (title_index(element_type(block)) < 0) { before = block; break; }
    }
    for (std::size_t next = i + 1; next < desired.size(); ++next) {
      if (desired[next].id.isEmpty()) continue;
      const auto block = block_with_id(document_, desired[next].id);
      if (!before.isValid() || block.position() < before.position()) before = block;
      break;
    }
    QTextBlockFormat format;
    format.setProperty(element_type_property, static_cast<int>(entry.type));
    format.setProperty(element_id_property, new_id());
    if (before.isValid()) {
      const int position = before.position();
      cursor.setPosition(position);
      cursor.insertBlock(before.blockFormat(), before.charFormat());
      cursor.setPosition(position);
      cursor.setBlockFormat(format);
      cursor.setBlockCharFormat(QTextCharFormat());
    } else {
      cursor.movePosition(QTextCursor::End);
      cursor.insertBlock(format, QTextCharFormat());
    }
    cursor.insertText(entry.text, QTextCharFormat());
  }
  // Descending removal avoids disturbing earlier title positions. Looking up
  // ids afresh also handles Qt merging a removed block with its successor.
  for (auto id = removed.crbegin(); id != removed.crend(); ++id) {
    const auto block = block_with_id(document_, *id);
    cursor.setPosition(block.position());
    remove_element_block(cursor, block);
  }
  cursor.endEditBlock();
  return true;
}

void EditorCommands::setPageBreakMessage(const QString& message) {
  if (message == page_break_message_) return;
  page_break_message_ = message;
  emit pageBreakMessageChanged();
}

bool EditorCommands::isPageBreak(int position) const {
  const auto block = document_->findBlock(position);
  return block.isValid() && element_type(block) == ElementType::page_break;
}

bool EditorCommands::refuseMarkerSelection(int start, int end) {
  const auto [from, to] = std::minmax(start, end);
  if (from == to) return false;
  for (auto block = document_->findBlock(from); block.isValid() && block.position() <= to; block = block.next()) {
    if (element_type(block) != ElementType::page_break) continue;
    setPageBreakMessage(QStringLiteral("This selection crosses a page break. Remove the page break first, or select text on one side."));
    return true;
  }
  return false;
}

int EditorCommands::editablePosition(int position, bool backward) const {
  const auto block = document_->findBlock(position);
  if (!block.isValid() || element_type(block) != ElementType::page_break) return position;
  const int at = nearby_writing(block, backward);
  return at < 0 ? position : at;
}

void EditorCommands::moveToWriting(QTextCursor& cursor, bool skipTitle) {
  const auto type = element_type(cursor.block());
  if (type != ElementType::page_break && !(skipTitle && title_index(type) >= 0)) return;
  const int at = nearby_writing(cursor.block(), false);
  if (at >= 0) { cursor.setPosition(at); return; }
  cursor.movePosition(QTextCursor::End);
  cursor.insertBlock(element_format(ElementType::action), QTextCharFormat());
}

int EditorCommands::insertPageBreak(int position, int start, int end) {
  if (start != end) {
    setPageBreakMessage(QStringLiteral("Keep the selected writing: place a single caret to insert a page break."));
    return -1;
  }
  const auto block = document_->findBlock(position);
  if (!block.isValid()) return -1;
  if (title_index(element_type(block)) >= 0) {
    setPageBreakMessage(QStringLiteral("Page breaks belong in the screenplay body, not the title page."));
    return -1;
  }
  const auto [first, last] = writing_group(block);
  const bool after = block == last && position == block.position() + block.length() - 1 &&
                     element_type(block) != ElementType::page_break && !block.text().isEmpty();
  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  typed_end_ = typed_steps_ = -1;
  int caret;
  if (!after) {
    insert_before(cursor, first, ElementType::page_break);
    cursor.setPosition(position + 1);
    moveToWriting(cursor);
    caret = cursor.position();
  } else {
    auto next = last.next();
    while (next.isValid() && title_index(element_type(next)) >= 0) next = next.next();
    if (next.isValid()) insert_before(cursor, next, ElementType::page_break);
    else {
      cursor.movePosition(QTextCursor::End);
      cursor.insertBlock(element_format(ElementType::page_break), QTextCharFormat());
    }
    auto following = cursor.block().next();
    while (following.isValid() && (element_type(following) == ElementType::page_break ||
                                   title_index(element_type(following)) >= 0))
      following = following.next();
    if (following.isValid()) cursor.setPosition(following.position());
    else {
      cursor.movePosition(QTextCursor::End);
      cursor.insertBlock(element_format(ElementType::action), QTextCharFormat());
    }
    caret = cursor.position();
  }
  cursor.endEditBlock();
  setPageBreakMessage({});
  return caret;
}

int EditorCommands::removePageBreak(int position, int start, int end) {
  auto block = document_->findBlock(position);
  if (start != end) {
    block = selected_marker(document_, start, end);
    if (!block.isValid()) {
      setPageBreakMessage(QStringLiteral("Keep the selected writing: place the caret beside the page break to remove it."));
      return -1;
    }
  } else if (block.isValid() && element_type(block) != ElementType::page_break) {
    if (position == block.position() && block.previous().isValid() &&
        element_type(block.previous()) == ElementType::page_break) block = block.previous();
    else if (position == block.position() + block.length() - 1 && block.next().isValid() &&
             element_type(block.next()) == ElementType::page_break) block = block.next();
    else block = {};
  }
  if (!block.isValid() || element_type(block) != ElementType::page_break) {
    setPageBreakMessage(QStringLiteral("Place the caret on a page break or at the adjacent element boundary to remove it."));
    return -1;
  }
  if (!block.text().isEmpty()) {
    setPageBreakMessage(QStringLiteral("This legacy page break contains saved text and is retained for review. It cannot be removed as an empty marker."));
    return -1;
  }
  const auto previous = block.previous(), next = block.next();
  if (previous.isValid() && next.isValid() &&
      (element_type(previous) == ElementType::cue || speech_part(element_type(previous))) &&
      (speech_part(element_type(next)) ||
       (element_type(next) == ElementType::cue && next.blockFormat().boolProperty(dual_right_property)))) {
    setPageBreakMessage(QStringLiteral("Removing this break would attach dialogue to a different speech or create a dual pair. Resolve the surrounding speech structure first."));
    return -1;
  }
  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  typed_end_ = typed_steps_ = -1;
  remove_element_block(cursor, block);
  moveToWriting(cursor, true);
  cursor.endEditBlock();
  setPageBreakMessage({});
  return cursor.position();
}

int EditorCommands::deleteText(int position, int start, int end, bool backward) {
  setDualMessage({});
  if (start != end) {
    if (const auto marker = selected_marker(document_, start, end); marker.isValid()) {
      const int caret = removePageBreak(marker.position(), start, end);
      return caret < 0 ? -2 : caret;
    }
    if (refuseDualSelection(start, end)) return -2;
    return refuseMarkerSelection(start, end) ? -2 : -1;
  }
  const auto block = document_->findBlock(position);
  if (!block.isValid()) return -1;
  if (start == end && ((backward && position == block.position()) ||
      (!backward && position == block.position() + block.length() - 1))) {
    const auto adjacent = backward ? block.previous() : block.next();
    if (adjacent.isValid() && element_type(adjacent) != ElementType::page_break &&
        element_type(block) != ElementType::page_break && (dual_member(block) || dual_member(adjacent))) {
      setDualMessage(unpair_first);
      return -2;
    }
  }
  auto marker = block;
  if (element_type(marker) != ElementType::page_break) {
    if (backward && position == block.position()) marker = block.previous();
    else if (!backward && position == block.position() + block.length() - 1) marker = block.next();
    else return -1;
  }
  if (!marker.isValid() || element_type(marker) != ElementType::page_break) return -1;
  const int caret = removePageBreak(marker.position(), marker.position(), marker.position());
  return caret < 0 ? position : caret;
}

bool EditorCommands::eventFilter(QObject* watched, QEvent* event) {
  if (forwarding_input_) return false;
  if (event->type() != QEvent::InputMethod && event->type() != QEvent::MouseButtonPress &&
      event->type() != QEvent::MouseButtonRelease)
    return QObject::eventFilter(watched, event);
  if (event->type() != QEvent::InputMethod && static_cast<QMouseEvent*>(event)->button() != Qt::MiddleButton)
    return QObject::eventFilter(watched, event);
  const int position = watched->property("cursorPosition").toInt();
  const int start = watched->property("selectionStart").toInt(), end = watched->property("selectionEnd").toInt();
  if (event->type() == QEvent::InputMethod) {
    const auto* input = static_cast<QInputMethodEvent*>(event);
    // Qt removes the selection first, then applies replacement offsets at its
    // left edge. Map that effective range back to original document positions.
    const auto [left, right] = std::minmax(start, end);
    const int base = start == end ? position : left;
    auto original_position = [&](int at) { return at < left ? at : at + right - left; };
    const int from = original_position(base + input->replacementStart());
    const int to = original_position(base + input->replacementStart() + input->replacementLength());
    if (refuseMarkerSelection(start, end) || refuseDualSelection(start, end) ||
        (input->replacementLength() && (refuseMarkerSelection(from, to) || refuseDualSelection(from, to)))) {
      event->accept();
      return true;
    }
    if (dual_member(document_->findBlock(editablePosition(position))) && paragraph_break(input->commitString())) {
      setDualMessage(unpair_first);
      event->accept();
      return true;
    }
    if (input->replacementStart() && (isPageBreak(position) || isPageBreak(from))) {
      setPageBreakMessage(QStringLiteral("Input-method replacement cannot cross a page-break boundary."));
      event->accept();
      return true;
    }
    if (isPageBreak(position)) {
      auto preedit_event = [&](const QString& text, const QList<QInputMethodEvent::Attribute>& attributes) {
        QScopedValueRollback guard(forwarding_input_, true);
        QInputMethodEvent update(text, attributes);
        QGuiApplication::sendEvent(watched, &update);
      };
      const int adjacent = editablePosition(position);
      if (adjacent != position && !input->replacementLength()) {
        preedit_event({}, {});
        watched->setProperty("cursorPosition", adjacent);
      } else if (!input->commitString().isEmpty() || input->replacementLength()) {
        if (input->replacementLength())
          setPageBreakMessage(QStringLiteral("Input-method replacement cannot cross a page-break boundary."));
        else {
          preedit_event({}, {});
          watched->setProperty("cursorPosition", typeText(position, start, end, input->commitString()));
          // Clear the old marker's transient ink before moving the caret;
          // deliver any new preedit on the actual writing element.
          preedit_event(input->preeditString(), input->attributes());
        }
        event->accept();
        return true;
      }
    }
  } else {
    const auto* mouse = static_cast<QMouseEvent*>(event);
    const int target = document_->documentLayout()->hitTest(mouse->position(), Qt::FuzzyHit);
    if (dual_member(document_->findBlock(target)) || dual_member(document_->findBlock(position)) || refuseDualSelection(start, end)) {
      setDualMessage(QStringLiteral("Use Paste (Ctrl+V) to preserve the Dual Dialogue structure; primary-selection paste is unavailable in a pair."));
      event->accept();
      return true;
    }
    if (mouse->button() == Qt::MiddleButton &&
        (isPageBreak(position) || isPageBreak(target) || refuseMarkerSelection(start, end))) {
      setPageBreakMessage(QStringLiteral("Use Paste (Ctrl+V) beside a page break; primary-selection paste is unavailable on markers."));
      event->accept();
      return true;
    }
  }
  return QObject::eventFilter(watched, event);
}

int EditorCommands::typeText(int position, int selectionStart, int selectionEnd, const QString& text) {
  setDualMessage({});
  if (refuseMarkerSelection(selectionStart, selectionEnd) || refuseDualSelection(selectionStart, selectionEnd)) return -1;
  if (dual_member(document_->findBlock(editablePosition(position))) && paragraph_break(text)) {
    setDualMessage(unpair_first); return -1;
  }
  setPageBreakMessage({});
  auto cursor = selection(document_, position, selectionStart, selectionEnd);
  const bool word_start = !text.isEmpty() && !text.front().isSpace() &&
                          (cursor.atBlockStart() || document_->characterAt(cursor.selectionStart() - 1).isSpace());
  const bool group = !cursor.hasSelection() && !isPageBreak(position) && cursor.position() == typed_end_ &&
                     document_->availableUndoSteps() == typed_steps_ && !word_start;
  // Outside an edit block, consecutive inserts merge into the previous step.
  if (!group) cursor.beginEditBlock();
  moveToWriting(cursor);
  if (!cursor.hasSelection() && !cursor.atBlockStart() && !cursor.atBlockEnd() &&
      document_->characterAt(cursor.position() - 1) == QChar::LineSeparator) {
    // The break itself is plain. At the start of a continued authored line,
    // inherit the text being continued, not the separator's format. This
    // derives from document text and works identically after reopening.
    auto following = cursor;
    following.movePosition(QTextCursor::NextCharacter);
    cursor.insertText(text, following.charFormat());
  } else cursor.insertText(text);
  if (!group) cursor.endEditBlock();
  typed_end_ = cursor.position();
  typed_steps_ = document_->availableUndoSteps();
  return typed_end_;
}

int EditorCommands::enter(int position, int selectionStart, int selectionEnd) {
  setDualMessage({});
  if (refuseMarkerSelection(selectionStart, selectionEnd) || refuseDualSelection(selectionStart, selectionEnd)) return -1;
  setPageBreakMessage({});
  auto cursor = selection(document_, position, selectionStart, selectionEnd);
  if (dual_member(cursor.block())) {
    const auto block = cursor.block();
    const auto type = element_type(block);
    auto remaining = block.text();
    remaining.remove(cursor.selectionStart() - block.position(), cursor.selectionEnd() - cursor.selectionStart());
    const bool at_end = cursor.selectionEnd() == block.position() + block.length() - 1;
    const auto last = writing_group(block).second;
    // Enter within dialogue is an authored break. Cue/parenthetical at end
    // may add dialogue, and the pair's final end may add following action.
    const bool safe = (!remaining.isEmpty() && type == ElementType::dialogue && !at_end) ||
                      (type == ElementType::parenthetical && (remaining.isEmpty() || at_end)) ||
                      (type == ElementType::cue && !remaining.isEmpty() && at_end) ||
                      (type == ElementType::dialogue && !remaining.isEmpty() && at_end && block == last);
    if (!safe) { setDualMessage(unpair_first); return -1; }
  }
  cursor.beginEditBlock();
  if (isPageBreak(position) && !cursor.hasSelection()) {
    moveToWriting(cursor);
    cursor.endEditBlock();
    return cursor.position();
  }
  cursor.removeSelectedText();
  const auto block = cursor.block();
  const auto type = element_type(block);
  if (block.text().isEmpty()) {
    const auto next = type == ElementType::parenthetical && in_speech(block) ? ElementType::dialogue : ElementType::action;
    if (type != next) set_type(cursor, next);
  } else if (type == ElementType::dialogue && !cursor.atBlockEnd()) {
    cursor.insertText(QString(QChar::LineSeparator), QTextCharFormat());
  } else if (cursor.atBlockEnd()) {
    const bool dialogue = type == ElementType::cue || (type == ElementType::parenthetical && in_speech(block));
    QTextBlockFormat next;
    next.setProperty(element_type_property, static_cast<int>(dialogue ? ElementType::dialogue : ElementType::action));
    next.setProperty(element_id_property, new_id());
    cursor.insertBlock(next, QTextCharFormat());
  } else {
    // Splitting keeps the element type; the dual flag stays with the first
    // part. The id stays with the text unless the text splits in two.
    auto format = block.blockFormat();
    format.clearProperty(dual_right_property);
    const bool at_start = cursor.atBlockStart();
    if (!at_start) format.setProperty(element_id_property, new_id());
    cursor.insertBlock(format);
    if (at_start) {
      QTextCursor before(cursor.block().previous());
      QTextBlockFormat id;
      id.setProperty(element_id_property, new_id());
      before.mergeBlockFormat(id);
    }
  }
  cursor.endEditBlock();
  return cursor.position();
}

void EditorCommands::cycleType(int position, bool backward) {
  setDualMessage({});
  if (dual_member(document_->findBlock(position))) { setDualMessage(unpair_first); return; }
  if (isPageBreak(position)) {
    setPageBreakMessage(QStringLiteral("A page break is not a text element. Move to adjacent writing to change its type."));
    return;
  }
  setPageBreakMessage({});
  QTextCursor cursor(document_);
  cursor.setPosition(position);
  const auto type = element_type(cursor.block());
  const auto found = std::find(cycle.begin(), cycle.end(), type);
  // Types outside the cycle (lyrics, centered text, title fields) become action.
  auto next = ElementType::action;
  if (found != cycle.end()) {
    const auto index = static_cast<std::size_t>(found - cycle.begin());
    next = cycle[(index + (backward ? cycle.size() - 1 : 1)) % cycle.size()];
  }
  cursor.beginEditBlock();
  set_type(cursor, next);
  cursor.endEditBlock();
}

void EditorCommands::toggleEmphasis(int selectionStart, int selectionEnd, const QString& name) {
  const auto style = name == QLatin1String("bold") ? std::optional{Style::bold} :
                     name == QLatin1String("italic") ? std::optional{Style::italic} :
                     name == QLatin1String("underline") ? std::optional{Style::underline} : std::nullopt;
  const auto [start, end] = std::minmax(selectionStart, selectionEnd);
  if (!style || start < 0 || end >= document_->characterCount() || start == end) return;
  auto emphasized = [&](const QTextCharFormat& format) {
    if (*style == Style::bold) return format.fontWeight() >= QFont::Bold;
    if (*style == Style::italic) return format.fontItalic();
    return format.fontUnderline();
  };
  bool all = true, any = false;
  for (auto block = document_->findBlock(start); block.isValid() && block.position() < end && all; block = block.next()) {
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      const auto fragment = it.fragment();
      const int from = std::max(start - fragment.position(), 0);
      const int to = std::min(end - fragment.position(), fragment.length());
      if (from >= to) continue;
      const auto text = fragment.text();
      // A separator has no authored emphasis. Ignore it when deciding
      // whether the selected text is already fully styled.
      for (int at = from; at < to; ++at) {
        if (text[at] == QChar::LineSeparator) continue;
        any = true;
        all &= emphasized(fragment.charFormat());
        break;
      }
      if (!all) break;
    }
  }
  if (!any) return;
  QTextCharFormat format;
  if (*style == Style::bold) format.setFontWeight(all ? QFont::Normal : QFont::Bold);
  else if (*style == Style::italic) format.setFontItalic(!all);
  else format.setFontUnderline(!all);
  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  for (auto block = document_->findBlock(start); block.isValid() && block.position() < end; block = block.next()) {
    const int to = std::min(end - block.position(), block.length() - 1);
    int from = std::max(start - block.position(), 0);
    if (all && from < to) {
      // Clear the style from legacy spans that include a deliberate break
      // too; toggle-off must not leave an invisible emphasis-only separator.
      cursor.setPosition(block.position() + from);
      cursor.setPosition(block.position() + to, QTextCursor::KeepAnchor);
      cursor.mergeCharFormat(format);
      continue;
    }
    const auto text = block.text();
    while (from < to) {
      const auto separator = text.indexOf(QChar::LineSeparator, from);
      const int stop = separator < 0 ? to : std::min(to, static_cast<int>(separator));
      if (from < stop) {
        cursor.setPosition(block.position() + from);
        cursor.setPosition(block.position() + stop, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(format);
      }
      from = stop + 1;
    }
  }
  cursor.endEditBlock();
  typed_end_ = typed_steps_ = -1;
}

void EditorCommands::copy(int selectionStart, int selectionEnd) {
  if (selectionStart == selectionEnd) return;
  const auto elements = selected_elements(document_, selectionStart, selectionEnd);
  QStringList paragraphs;
  for (const auto& element : elements) paragraphs.push_back(QString::fromStdString(element.text));
  auto* data = new QMimeData;
  data->setText(paragraphs.join(QLatin1Char('\n')));
  data->setData(mime_type, serialize(elements));
  QGuiApplication::clipboard()->setMimeData(data);
}

int EditorCommands::cut(int selectionStart, int selectionEnd) {
  setDualMessage({});
  if (refuseMarkerSelection(selectionStart, selectionEnd) || refuseDualSelection(selectionStart, selectionEnd)) return -1;
  setPageBreakMessage({});
  copy(selectionStart, selectionEnd);
  auto cursor = selection(document_, selectionStart, selectionStart, selectionEnd);
  cursor.beginEditBlock();
  cursor.removeSelectedText();
  cursor.endEditBlock();
  return cursor.position();
}

int EditorCommands::paste(int position, int selectionStart, int selectionEnd) {
  setDualMessage({});
  if (refuseMarkerSelection(selectionStart, selectionEnd) || refuseDualSelection(selectionStart, selectionEnd)) return -1;
  setPageBreakMessage({});
  const auto* data = QGuiApplication::clipboard()->mimeData();
  if (!data) return position;
  std::vector<Element> elements;
  bool apollo_selection = false;
  if (data->hasFormat(mime_type)) {
    if (auto decoded = deserialize(data->data(mime_type))) {
      elements = std::move(*decoded);
      apollo_selection = true;
    }
  }
  if (std::any_of(elements.begin(), elements.end(), [](const Element& e) { return e.type == ElementType::page_break; })) {
    setPageBreakMessage(QStringLiteral("This Apollo selection contains page breaks. Paste text from one side, then use Insert page break to preserve speech boundaries."));
    return -1;
  }
  if (!apollo_selection) {
    // Plain text: one action paragraph per non-empty line, or dialogue when
    // pasted inside dialogue. Line separators and tabs never reach the layout.
    const int target = selectionStart != selectionEnd ? std::min(selectionStart, selectionEnd) : editablePosition(position);
    const auto type = element_type(document_->findBlock(target));
    for (auto line : data->text().split(QRegularExpression(QStringLiteral("\\r\\n|[\\r\\n\\x{2028}\\x{2029}]")))) {
      line.replace(QLatin1Char('\t'), QLatin1Char(' '));
      if (!line.trimmed().isEmpty())
        elements.push_back({type == ElementType::dialogue ? ElementType::dialogue : ElementType::action, line.toStdString()});
    }
  }
  if (elements.empty()) return position;
  const auto destination = document_->findBlock(selectionStart != selectionEnd ? std::min(selectionStart, selectionEnd) : editablePosition(position));
  if (dual_member(destination) && (elements.size() != 1 || (apollo_selection &&
      (elements.front().type != element_type(destination) || elements.front().dual_right != block_element(destination).dual_right)))) {
    setDualMessage(unpair_first); return -1;
  }
  auto cursor = selection(document_, position, selectionStart, selectionEnd);
  cursor.beginEditBlock();
  moveToWriting(cursor);
  cursor.removeSelectedText();
  int caret;
  const auto target = cursor.block();
  const bool same_block_type = elements.front().type == element_type(target) &&
                              elements.front().dual_right == target.blockFormat().boolProperty(dual_right_property);
  if (elements.size() == 1 && (!apollo_selection || same_block_type)) {
    // Plain text joins inline. An Apollo element can join only when the
    // destination already has its block properties; otherwise insert_between
    // gives it a block of its own, including when the destination is empty.
    insert_runs(cursor, elements.front());
    caret = cursor.position();
  } else {
    caret = insert_between(cursor, elements);
  }
  cursor.endEditBlock();
  return caret;
}

int EditorCommands::moveSceneUp(int position) {
  auto heading = document_->findBlock(position);
  while (heading.isValid() && element_type(heading) != ElementType::heading) heading = heading.previous();
  if (!heading.isValid()) return -1;
  auto previous = heading.previous();
  while (previous.isValid() && element_type(previous) != ElementType::heading) previous = previous.previous();
  if (!previous.isValid()) return -1;
  auto next = heading.next();
  while (next.isValid() && element_type(next) != ElementType::heading) next = next.next();
  std::vector<Element> scene, earlier;
  for (auto block = heading; block != next; block = block.next()) scene.push_back(block_element(block));
  for (auto block = previous; block != heading; block = block.next()) earlier.push_back(block_element(block));
  const int start = previous.position(), offset = position - heading.position();
  const int end = next.isValid() ? next.position() - 1 : document_->characterCount() - 1;
  scene.insert(scene.end(), earlier.begin(), earlier.end());
  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  cursor.setPosition(start);
  cursor.setPosition(end, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  insert_elements(cursor, scene);
  cursor.endEditBlock();
  return start + offset;
}
}
