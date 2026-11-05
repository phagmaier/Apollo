#include "editor/find_replace.h"
#include "editor/screenplay_layout.h"

#include <QElapsedTimer>
#include <algorithm>

namespace apollo::editor {
namespace {
// A literal field cannot insert paragraph boundaries, tabs or source control
// characters. Deliberate breaks already in writing remain separate lines.
bool valid_field(const QString& text) {
  if (!text.isValidUtf16() || text.size() > 4096) return false;
  for (const auto ch : text)
    if (ch.category() == QChar::Other_Control || ch == QChar::LineSeparator ||
        ch == QChar::ParagraphSeparator || ch.unicode() == 0xfdd0 || ch.unicode() == 0xfdd1) return false;
  return true;
}
bool word_character(char32_t ch) {
  const auto category = QChar::category(ch);
  return QChar::isLetterOrNumber(ch) || ch == U'_' || category == QChar::Mark_NonSpacing ||
         category == QChar::Mark_SpacingCombining || category == QChar::Mark_Enclosing;
}
bool word_at(const QString& text, int at) {
  if (at < 0 || at >= text.size()) return false;
  if (text[at].isLowSurrogate() && at > 0 && text[at - 1].isHighSurrogate()) --at;
  const auto ch = text[at];
  return word_character(ch.isHighSurrogate() && at + 1 < text.size() && text[at + 1].isLowSurrogate()
      ? QChar::surrogateToUcs4(ch, text[at + 1]) : ch.unicode());
}
}

FindReplace::FindReplace(QTextDocument* document, QObject* parent)
    : QObject(parent), document_(document), scope_start_(document), scope_end_(document), origin_(document) {
  scope_start_.setKeepPositionOnInsert(true);
  timer_.setSingleShot(true);
  connect(&timer_, &QTimer::timeout, this, &FindReplace::scan);
  connect(document_, &QTextDocument::contentsChanged, this, [this] {
    if (active_ && !changing_) { message_.clear(); restart(); }
  });
}

QString FindReplace::status() const {
  if (busy_) return QStringLiteral("Searching…");
  if (!message_.isEmpty()) return message_;
  if (query_.isEmpty()) return QStringLiteral("Enter text to find.");
  if (matches_.empty()) return QStringLiteral("No matches");
  return QStringLiteral("%1 of %2 matches").arg(current()).arg(count());
}

void FindReplace::begin(int position, int start, int end) {
  const int last = document_->characterCount() - 1;
  scope_start_.setPosition(std::clamp(std::min(start, end), 0, last));
  scope_end_.setPosition(std::clamp(std::max(start, end), 0, last));
  captured_scope_ = scope_start_.position() < scope_end_.position();
  origin_.setPosition(std::clamp(position, 0, last));
  active_ = true;
  message_.clear();
  restart();
}

void FindReplace::configure(const QString& query, bool matchCase, bool wholeWords, bool selectionOnly) {
  query_ = query;
  match_case_ = matchCase;
  whole_words_ = wholeWords;
  selection_only_ = selectionOnly;
  message_.clear();
  restart();
}

void FindReplace::dismiss() {
  active_ = false;
  timer_.stop();
  busy_ = false;
  matches_.clear(); pending_.clear(); selected_ = -1;
  emit changed();
}

void FindReplace::restart() {
  timer_.stop();
  revision_ = document_->revision();
  matches_.clear(); pending_.clear(); selected_ = -1;
  busy_ = false;
  if (active_ && !query_.isEmpty()) {
    if (!valid_field(query_)) message_ = QStringLiteral("Find text must be one line, at most 4096 characters, without tabs or controls.");
    else if (selection_only_ && !hasScope()) message_ = QStringLiteral("Select writing before opening Find to search a selection.");
    else {
      block_ = document_->findBlock(selection_only_ ? scope_start_.position() : 0);
      offset_ = 0;
      busy_ = true;
      timer_.start(0);
    }
  }
  emit changed();
}

void FindReplace::scan() {
  if (!active_ || revision_ != document_->revision()) return;
  QElapsedTimer elapsed; elapsed.start();
  const int from = selection_only_ ? scope_start_.position() : 0;
  const int to = selection_only_ ? scope_end_.position() : document_->characterCount() - 1;
  int blocks = 0, attempts = 0;
  while (block_.isValid() && block_.position() < to) {
    const auto type = element_type(block_);
    if (type != ElementType::page_break && type != ElementType::more) {
      const auto text = block_.text();
      const int end = std::min(static_cast<int>(text.size()), to - block_.position());
      offset_ = std::max(offset_, from - block_.position());
      // Read the block once per batch. Copying it for each dense match makes
      // long paragraphs quadratic, especially under the sanitizers.
      while (offset_ < end) {
        const int limit = std::min(end, offset_ + 8192);
        const auto chunk = QStringView(text).mid(offset_, std::min(end - offset_, 8192 + static_cast<int>(query_.size())));
        const auto relative = chunk.indexOf(query_, 0, match_case_ ? Qt::CaseSensitive : Qt::CaseInsensitive);
        const int at = relative < 0 ? -1 : offset_ + static_cast<int>(relative);
        if (at >= 0 && at < limit && at + query_.size() <= end) {
          const bool accepted = !whole_words_ || (!word_at(text, at - 1) && !word_at(text, at + static_cast<int>(query_.size())));
          if (accepted)
            pending_.push_back({block_.position() + at, block_.position() + at + static_cast<int>(query_.size())});
          offset_ = at + (accepted ? static_cast<int>(query_.size()) : 1);
        } else offset_ = limit;
        if (pending_.size() > 100000) {
          pending_.clear(); busy_ = false;
          message_ = QStringLiteral("More than 100,000 matches. Narrow the search or select a smaller range.");
          emit changed(); return;
        }
        if (++attempts >= 512 || elapsed.elapsed() >= 4) { timer_.start(0); return; }
      }
    }
    block_ = block_.next(); offset_ = 0;
    if (++blocks >= 128 || elapsed.elapsed() >= 4) { timer_.start(0); return; }
  }
  busy_ = false;
  matches_ = std::move(pending_);
  if (!matches_.empty()) {
    auto it = std::lower_bound(matches_.begin(), matches_.end(), origin_.position(),
        [](const Match& match, int position) { return match.from < position; });
    selected_ = it == matches_.end() ? 0 : static_cast<int>(it - matches_.begin());
  }
  select();
}

bool FindReplace::ready() const {
  return active_ && !busy_ && revision_ == document_->revision() && selected_ >= 0 && selected_ < count();
}

void FindReplace::select() {
  emit changed();
  if (ready()) emit selectionRequested(matches_[selected_].from, matches_[selected_].to);
}

void FindReplace::next(bool backward) {
  if (!ready()) return;
  message_.clear();
  selected_ = (selected_ + count() + (backward ? -1 : 1)) % count();
  origin_.setPosition(matches_[selected_].from);
  select();
}

int FindReplace::replace(const QString& replacement, bool all) {
  if (!ready()) return 0;
  if (!valid_field(replacement)) {
    message_ = QStringLiteral("Replacement must be one line, at most 4096 characters, without tabs or controls.");
    emit changed(); return 0;
  }
  // Validate every range before touching writing. Reverse order keeps positions
  // stable; one edit block restores exact styles, ids and flags with one Undo.
  const int first = all ? 0 : selected_, last = all ? count() : selected_ + 1;
  if (qint64(last - first) * std::max(qsizetype(0), replacement.size() - query_.size()) > 1000000) {
    message_ = QStringLiteral("Replacement would add more than 1,000,000 characters. Use a smaller selection or shorter replacement.");
    emit changed(); return 0;
  }
  for (int i = first; i < last; ++i) {
    const auto& match = matches_[i];
    const auto block = document_->findBlock(match.from);
    if (!block.isValid() || match.to > block.position() + block.length() - 1 ||
        block.text().mid(match.from - block.position(), match.to - match.from).compare(
            query_, match_case_ ? Qt::CaseSensitive : Qt::CaseInsensitive) != 0) {
      restart(); return 0;
    }
  }
  origin_.setPosition(matches_[selected_].to);
  changing_ = true;
  QTextCursor cursor(document_);
  cursor.beginEditBlock();
  int changed = 0;
  for (int i = last - 1; i >= first; --i) {
    const auto match = matches_[i];
    cursor.setPosition(match.from);
    cursor.setPosition(match.to, QTextCursor::KeepAnchor);
    if (cursor.selectedText() == replacement) continue;
    QTextCursor style(document_);
    style.setPosition(match.from); style.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    cursor.insertText(replacement, style.charFormat());
    ++changed;
  }
  cursor.endEditBlock();
  changing_ = false;
  restart();
  // Search remains usable immediately after a no-op as well.
  return changed;
}
}
