#include "editor/editor_commands.h"
#include "editor/screenplay_layout.h"
#include "source/screenplay_format.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
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

// The elements covering [start, end), each cut to the selected part. A
// selection edge that covers none of a block's text leaves that block out.
std::vector<Element> selected_elements(QTextDocument* document, int start, int end) {
  std::vector<Element> elements;
  const auto first = document->findBlock(start), last = document->findBlock(end);
  for (auto block = first; block.isValid(); block = block.next()) {
    const auto text = block.text();
    const int from = std::max(start - block.position(), 0), to = std::min(end - block.position(), static_cast<int>(text.size()));
    const bool edge_only = first != last && ((block == first && from == text.size()) || (block == last && to == 0));
    if (!edge_only)
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

EditorCommands::EditorCommands(QTextDocument* document, QObject* parent) : QObject(parent), document_(document) {}

int EditorCommands::typeText(int position, int selectionStart, int selectionEnd, const QString& text) {
  auto cursor = selection(document_, position, selectionStart, selectionEnd);
  const bool word_start = !text.isEmpty() && !text.front().isSpace() &&
                          (cursor.atBlockStart() || document_->characterAt(cursor.selectionStart() - 1).isSpace());
  const bool group = !cursor.hasSelection() && cursor.position() == typed_end_ &&
                     document_->availableUndoSteps() == typed_steps_ && !word_start;
  // Outside an edit block, consecutive inserts merge into the previous step.
  if (!group) cursor.beginEditBlock();
  cursor.insertText(text);
  if (!group) cursor.endEditBlock();
  typed_end_ = cursor.position();
  typed_steps_ = document_->availableUndoSteps();
  return typed_end_;
}

int EditorCommands::enter(int position, int selectionStart, int selectionEnd) {
  auto cursor = selection(document_, position, selectionStart, selectionEnd);
  cursor.beginEditBlock();
  cursor.removeSelectedText();
  const auto block = cursor.block();
  const auto type = element_type(block);
  if (block.text().isEmpty()) {
    // Provisional: Enter on an empty element makes it action.
    if (type != ElementType::action) set_type(cursor, ElementType::action);
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
  copy(selectionStart, selectionEnd);
  auto cursor = selection(document_, selectionStart, selectionStart, selectionEnd);
  cursor.beginEditBlock();
  cursor.removeSelectedText();
  cursor.endEditBlock();
  return cursor.position();
}

int EditorCommands::paste(int position, int selectionStart, int selectionEnd) {
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
  if (!apollo_selection) {
    // Plain text: one action paragraph per non-empty line, or dialogue when
    // pasted inside dialogue. Line separators and tabs never reach the layout.
    const auto type = element_type(document_->findBlock(selectionStart != selectionEnd ? selectionStart : position));
    for (auto line : data->text().split(QRegularExpression(QStringLiteral("\\r\\n|[\\r\\n\\x{2028}\\x{2029}]")))) {
      line.replace(QLatin1Char('\t'), QLatin1Char(' '));
      if (!line.trimmed().isEmpty())
        elements.push_back({type == ElementType::dialogue ? ElementType::dialogue : ElementType::action, line.toStdString()});
    }
  }
  if (elements.empty()) return position;
  auto cursor = selection(document_, position, selectionStart, selectionEnd);
  cursor.beginEditBlock();
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
