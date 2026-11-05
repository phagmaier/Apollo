#include "editor/writing_suggestions.h"
#include "editor/screenplay_layout.h"

#include <QRegularExpression>
#include <QTextCursor>
#include <algorithm>

namespace apollo::editor {
namespace {
struct Heading { int location = -1, time = -1; };
Heading heading_parts(const QString& text) {
  static const QRegularExpression prefix(
      QStringLiteral("^(?:INT\\./EXT\\.|EXT\\./INT\\.|INT/EXT\\.|I/E\\.|INT\\.|EXT\\.)\\s+"),
      QRegularExpression::CaseInsensitiveOption);
  const auto match = prefix.match(text);
  if (!match.hasMatch()) return {};
  const int separator = static_cast<int>(text.lastIndexOf(QStringLiteral(" - ")));
  return {static_cast<int>(match.capturedLength()), separator >= match.capturedLength() ? separator : -1};
}
QString base_name(const QString& cue) {
  static const QRegularExpression qualified(QStringLiteral("^(.+?)(?:\\s*\\([^()]*\\))+\\s*$"));
  const auto match = qualified.match(cue);
  return (match.hasMatch() ? match.captured(1) : cue).trimmed().toUpper();
}
}

WritingSuggestions::WritingSuggestions(QTextDocument* document, QObject* parent)
    : QObject(parent), document_(document) {
  scan_.setInterval(0);
  connect(&scan_, &QTimer::timeout, this, &WritingSuggestions::scanChunk);
  connect(document_, &QTextDocument::contentsChanged, this, [this] { dismiss(); });
}

WritingSuggestions::Context WritingSuggestions::contextAt(int position) const {
  Context result;
  if (position < 0 || position >= document_->characterCount()) return result;
  const auto block = document_->findBlock(position);
  const auto text = block.text();
  if (text.contains(QChar::LineSeparator)) return result;
  const int offset = position - block.position();
  int from = 0, to = static_cast<int>(text.size()), closing = -1;
  const auto type = element_type(block);
  if (type == ElementType::cue) {
    const int paren = static_cast<int>(text.indexOf('('));
    if (paren >= 0 && offset > paren) {
      if (text.left(paren).trimmed().isEmpty() || text.indexOf('(', paren + 1) >= 0) return result;
      from = paren + 1;
      const int close = static_cast<int>(text.indexOf(')', from));
      closing = close;
      to = close < 0 ? to : close;
      result.kind = QStringLiteral("Extension");
      if (close < 0) result.closing = QStringLiteral(")");
    } else {
      to = paren < 0 ? to : paren;
      result.kind = QStringLiteral("Character");
    }
  } else if (type == ElementType::heading) {
    const auto parts = heading_parts(text);
    if (parts.location < 0 || offset < parts.location) {
      to = parts.location < 0 ? to : static_cast<int>(text.indexOf(' '));
      result.kind = QStringLiteral("Interior / exterior");
    } else if (parts.time >= 0 && offset >= parts.time + 3) {
      from = parts.time + 3;
      result.kind = QStringLiteral("Time of day");
    } else {
      from = parts.location;
      if (parts.time >= 0) to = parts.time;
      result.kind = QStringLiteral("Location");
    }
  } else return result;
  while (from < to && text[from].isSpace()) ++from;
  while (to > from && text[to - 1].isSpace()) --to;
  if (offset < from || offset > to) return {};
  result.prefix = text.mid(from, offset - from).toUpper();
  if (result.prefix.isEmpty() && (result.kind == "Character" || result.kind == "Interior / exterior")) return {};
  const auto id = block.blockFormat().stringProperty(element_id_property);
  result.key = (id.isEmpty() ? QString::number(block.position()) : id) + ':' + result.kind;
  result.text = text.mid(from, to - from);
  result.from = block.position() + from;
  result.to = block.position() + to;
  result.following = closing < 0 ? 0 : closing - to + 1;
  result.position = position;
  return result;
}

void WritingSuggestions::dismiss(bool suppress) {
  if (suppress) suppressed_ = context_.key;
  scan_.stop();
  if (choices_.isEmpty()) return;
  choices_.clear();
  selected_ = 0;
  emit changed();
}

void WritingSuggestions::cursorChanged(int position) {
  dismiss();
  if (contextAt(position).key != suppressed_) suppressed_.clear();
}

void WritingSuggestions::request(int position, int start, int end, bool explicitRequest) {
  dismiss();
  context_ = start == end ? contextAt(position) : Context{};
  if (context_.key != suppressed_ || explicitRequest) suppressed_.clear();
  if (context_.key.isEmpty() || context_.key == suppressed_) return;
  revision_ = document_->revision();
  frequency_.clear();
  auto add = [this](const QString& kind, const QStringList& values) {
    for (const auto& value : values) addCandidate(kind, value);
  };
  add(QStringLiteral("Interior / exterior"), {"INT.", "EXT.", "INT./EXT."});
  add(QStringLiteral("Extension"), {"V.O.", "O.S.", "CONT'D"});
  add(QStringLiteral("Time of day"), {"DAY", "NIGHT", "MORNING", "AFTERNOON", "EVENING", "LATER", "CONTINUOUS"});
  next_ = document_->begin();
  scan_.start();
}

void WritingSuggestions::addCandidate(const QString& kind, const QString& text) {
  if (kind == context_.kind && !text.isEmpty() && text.startsWith(context_.prefix) &&
      (text != context_.text || !context_.closing.isEmpty())) ++frequency_[text];
}

void WritingSuggestions::scanChunk() {
  if (revision_ != document_->revision()) { dismiss(); return; }
  // Yield between bounded batches. Never serialize/extract the full screenplay
  // on the input event, and never publish results from an obsolete revision.
  for (int n = 0; n < 128 && next_.isValid(); ++n, next_ = next_.next()) {
    if (next_ == document_->findBlock(context_.position)) continue;
    const auto type = element_type(next_);
    if (type != ElementType::cue && type != ElementType::heading) continue;
    const auto text = next_.text();
    if (text.contains(QChar::LineSeparator)) continue;
    if (type == ElementType::cue) {
      const auto base = base_name(text);
      addCandidate(QStringLiteral("Character"), base);
      static const QRegularExpression extensions(QStringLiteral("\\(([^()]*)\\)"));
      auto matches = extensions.globalMatch(text);
      while (matches.hasNext()) {
        const auto extension = matches.next().captured(1).trimmed().toUpper();
        addCandidate(QStringLiteral("Extension"), extension);
      }
    } else {
      const auto parts = heading_parts(text);
      if (parts.location < 0) continue;
      const auto location = text.mid(parts.location, parts.time < 0 ? -1 : parts.time - parts.location).trimmed().toUpper();
      addCandidate(QStringLiteral("Location"), location);
      if (parts.time >= 0) {
        const auto time = text.mid(parts.time + 3).trimmed().toUpper();
        addCandidate(QStringLiteral("Time of day"), time);
      }
    }
  }
  if (next_.isValid()) return;
  scan_.stop();
  choices_ = frequency_.keys();
  std::sort(choices_.begin(), choices_.end(), [&](const auto& a, const auto& b) {
    return frequency_[a] == frequency_[b] ? a < b : frequency_[a] > frequency_[b];
  });
  // The menu is intentionally bounded; narrowing the prefix finds later names.
  if (choices_.size() > 12) choices_ = choices_.mid(0, 12);
  selected_ = 0;
  emit changed();
}

void WritingSuggestions::move(int delta) {
  if (choices_.isEmpty()) return;
  selected_ = (selected_ + delta + static_cast<int>(choices_.size())) % static_cast<int>(choices_.size());
  emit changed();
}

void WritingSuggestions::select(int index) {
  if (index < 0 || index >= choices_.size()) return;
  selected_ = index;
  emit changed();
}

int WritingSuggestions::accept(int position, int start, int end) {
  const auto current = contextAt(position);
  if (choices_.isEmpty() || start != end || revision_ != document_->revision() ||
      position != context_.position || current.key != context_.key || current.text != context_.text) {
    dismiss(); return -1;
  }
  const auto replacement = choices_[selected_] + context_.closing;
  const auto old = context_.text;
  const int from = context_.from, to = context_.to;
  const int following = context_.following;
  dismiss();
  // Retain already authored matching prefix/suffix and their emphasis. Only
  // the changed range is replaced, using the format at that range.
  int prefix = 0, suffix = 0;
  while (prefix < old.size() && prefix < replacement.size() && old[prefix] == replacement[prefix]) ++prefix;
  if (prefix && old[prefix - 1].isHighSurrogate()) --prefix;
  while (suffix < old.size() - prefix && suffix < replacement.size() - prefix &&
         old[old.size() - suffix - 1] == replacement[replacement.size() - suffix - 1]) ++suffix;
  if (suffix && old[old.size() - suffix].isLowSurrogate()) --suffix;
  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  cursor.setPosition(from + prefix);
  cursor.setPosition(to - suffix, QTextCursor::KeepAnchor);
  cursor.insertText(replacement.mid(prefix, replacement.size() - prefix - suffix));
  cursor.endEditBlock();
  return from + static_cast<int>(replacement.size()) + following;
}
}
