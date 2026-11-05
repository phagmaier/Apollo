#include "editor/spelling.h"
#include "app/runtime_assets.h"
#include "editor/screenplay_layout.h"
#include <hunspell.hxx>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextLayout>
#include <cmath>

namespace apollo::editor {
namespace {
QString key(QString word) { return word.replace(QChar(0x2019), QChar('\'')).normalized(QString::NormalizationForm_C).toCaseFolded(); }
const QRegularExpression& words() {
  static const QRegularExpression pattern(QStringLiteral("[\\p{L}\\p{M}]+(?:['’][\\p{L}\\p{M}]+)*"));
  return pattern;
}
}
Spelling::Spelling(QTextDocument* document, const QString& personalPath, QObject* parent)
    : QObject(parent), document_(document), worker_(new QObject),
      path_(personalPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                                   + QStringLiteral("/apollo/personal-dictionary.json") : personalPath) {
  loadPersonal();
  worker_->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
  thread_.start();
  QMetaObject::invokeMethod(worker_, [this] {
    const QString base = runtime_asset(QStringLiteral("spelling/en_US"));
    const bool available = QFile::exists(base + ".aff") && QFile::exists(base + ".dic");
    if (available) checker_ = std::make_shared<Hunspell>((base + ".aff").toUtf8().constData(), (base + ".dic").toUtf8().constData());
    QMetaObject::invokeMethod(this, [this, available] {
      ready_ = available;
      if (!available) { error_ = "US English dictionary unavailable; spelling is disabled."; busy_ = false; emit changed(); }
      else restart();
    }, Qt::QueuedConnection);
  });
  debounce_.setSingleShot(true);
  debounce_.setInterval(180);
  connect(&debounce_, &QTimer::timeout, this, [this] { next_ = document_->begin(); pending_.clear(); scanNext(); });
  connect(document_, &QTextDocument::contentsChanged, this, &Spelling::restart);
  reload_.setInterval(1500);
  connect(&reload_, &QTimer::timeout, this, [this] { if (!ready_) return; const auto old = personal_; loadPersonal(); if (old != personal_) restart(); });
  reload_.start();
}
Spelling::~Spelling() { thread_.quit(); thread_.wait(); }

bool Spelling::loadPersonal() {
  QFile file(path_);
  if (!file.exists()) { personal_.clear(); personal_bytes_.clear(); if (!error_.isEmpty()) { error_.clear(); emit changed(); } return true; }
  if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
    error_ = "Cannot read personal dictionary: " + path_; emit changed(); return false;
  }
  const auto bytes = file.readAll();
  if (!bytes.isEmpty() && bytes == personal_bytes_) { if (!error_.isEmpty()) { error_.clear(); emit changed(); } return true; }
  QJsonParseError parse;
  const auto json = QJsonDocument::fromJson(bytes, &parse);
  if (parse.error != QJsonParseError::NoError || !json.isArray()) {
    error_ = "Invalid personal dictionary; preserved at " + path_; emit changed(); return false;
  }
  QSet<QString> loaded;
  for (const auto& value : json.array()) {
    const auto word = value.toString();
    const auto match = words().match(word);
    if (!value.isString() || word.size() > 128 || !match.hasMatch() || match.capturedLength() != word.size()) {
      error_ = "Invalid personal dictionary; preserved at " + path_; emit changed(); return false;
    }
    loaded.insert(key(word));
  }
  personal_ = loaded; personal_bytes_ = bytes;
  error_.clear(); emit changed(); return true;
}
void Spelling::restart() {
  ++generation_;
  dismiss();
  issues_.clear(); pending_.clear(); next_ = {};
  busy_ = ready_;
  emit changed();
  if (ready_) debounce_.start();
}
void Spelling::scanNext() {
  if (in_flight_ || debounce_.isActive() || !ready_) return;
  if (!next_.isValid()) { issues_ = pending_; published_revision_ = document_->revision(); busy_ = false; emit changed(); return; }
  const auto block = next_;
  next_ = next_.next();
  const int generation = generation_, position = block.position();
  const auto text = element_type(block) == ElementType::page_break ? QString() : block.text();
  const auto personal = personal_;
  in_flight_ = true;
  QMetaObject::invokeMethod(worker_, [this, generation, position, text, personal] {
    QVariantList found;
    auto matches = words().globalMatch(text);
    while (matches.hasNext()) {
      const auto match = matches.next();
      const auto word = match.captured();
      // Bound Hunspell's work for pathological invented tokens. Such tokens
      // remain authored text; no replacement can be inferred safely.
      if (word.size() > 128 || personal.contains(key(word))) continue;
      const auto bytes = word.toUtf8().toStdString();
      if (!checker_->spell(bytes)) found.append(QVariantMap{{"position", position + int(match.capturedStart())},
          {"length", int(word.size())}, {"word", word}});
    }
    QMetaObject::invokeMethod(this, [this, generation, found] {
      in_flight_ = false;
      if (generation == generation_) pending_.append(found);
      scanNext();
    }, Qt::QueuedConnection);
  });
}
void Spelling::dismiss() {
  ++menu_generation_; word_.clear(); choices_.clear(); message_.clear(); from_ = -1; emit menuChanged();
}
void Spelling::request(int position, int start, int end) {
  dismiss();
  if (!ready_ || start != end || position < 0 || position >= document_->characterCount()) return;
  const auto block = document_->findBlock(position);
  if (element_type(block) == ElementType::page_break) return;
  auto matches = words().globalMatch(block.text());
  while (matches.hasNext()) {
    const auto match = matches.next();
    const int from = block.position() + int(match.capturedStart()), to = from + int(match.capturedLength());
    if (position < from || position > to) continue;
    word_ = match.captured(); from_ = from; menu_revision_ = document_->revision();
    const int generation = menu_generation_;
    const auto word = word_;
    const bool personal = personal_.contains(key(word));
    message_ = "Checking…"; emit menuChanged();
    QMetaObject::invokeMethod(worker_, [this, word, generation, personal] {
      const bool correct = personal || word.size() > 128 || checker_->spell(word.toUtf8().toStdString());
      QStringList suggestions;
      if (!correct) {
        for (const auto& suggestion : checker_->suggest(word.toUtf8().toStdString())) {
          suggestions.append(QString::fromUtf8(suggestion));
          if (suggestions.size() == 12) break;
        }
      }
      QMetaObject::invokeMethod(this, [this, generation, suggestions, correct] {
        if (generation != menu_generation_ || !validMenu()) return;
        choices_ = suggestions;
        message_ = word_.size() > 128 ? "Word is too long to check." : correct ? "Recognized word." : choices_.isEmpty() ? "No replacements found." : "Choose a replacement, or add this word.";
        emit menuChanged();
      }, Qt::QueuedConnection);
    });
    return;
  }
}
bool Spelling::validMenu() const {
  if (from_ < 0 || word_.isEmpty() || menu_revision_ != document_->revision()) return false;
  QTextCursor cursor(document_); cursor.setPosition(from_); cursor.setPosition(from_ + int(word_.size()), QTextCursor::KeepAnchor);
  return cursor.selectedText() == word_;
}
int Spelling::replace(const QString& replacement) {
  if (!validMenu() || !choices_.contains(replacement)) { dismiss(); return -1; }
  const int from = from_, size = int(word_.size());
  // Keep matching prefix/suffix formats, replacing only changed letters.
  int prefix = 0, suffix = 0;
  while (prefix < size && prefix < replacement.size() && word_[prefix] == replacement[prefix]) ++prefix;
  if (prefix && word_[prefix - 1].isHighSurrogate()) --prefix;
  while (suffix < size - prefix && suffix < replacement.size() - prefix && word_[size - suffix - 1] == replacement[replacement.size() - suffix - 1]) ++suffix;
  if (suffix && word_[size - suffix].isLowSurrogate()) --suffix;
  QTextCursor cursor(document_); cursor.beginEditBlock(); cursor.setPosition(from + prefix);
  cursor.setPosition(from + size - suffix, QTextCursor::KeepAnchor);
  cursor.insertText(replacement.mid(prefix, replacement.size() - prefix - suffix)); cursor.endEditBlock();
  return from + int(replacement.size());
}
bool Spelling::addWord() {
  if (!validMenu() || word_.size() > 128) return false;
  const auto word = key(word_);
  if (!QDir().mkpath(QFileInfo(path_).absolutePath())) { error_ = "Cannot create personal dictionary folder."; emit changed(); return false; }
  QLockFile lock(path_ + ".lock");
  if (!lock.tryLock(0)) { error_ = "Personal dictionary is busy; try Add again."; emit changed(); return false; }
  if (!loadPersonal()) return false;
  auto merged = personal_; merged.insert(word);
  auto list = merged.values(); std::sort(list.begin(), list.end());
  QJsonArray array; for (const auto& item : list) array.append(item);
  const auto bytes = QJsonDocument(array).toJson();
  QSaveFile file(path_);
  if (bytes.size() > 1024 * 1024 || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
    error_ = "Could not save personal dictionary; word was not added."; emit changed(); return false;
  }
  personal_ = merged; personal_bytes_ = bytes; error_.clear(); restart(); return true;
}
QVariantList Spelling::marks(qreal top, qreal bottom) const {
  QVariantList result;
  if (published_revision_ != document_->revision()) return result;
  for (const auto& issue : issues_) {
    const auto value = issue.toMap(); const int position = value["position"].toInt(), length = value["length"].toInt();
    const auto block = document_->findBlock(position);
    if (!block.isValid()) continue;
    const int from = position - block.position(), to = from + length;
    if (block.text().mid(from, length) != value["word"].toString()) continue;
    auto* layout = block.layout();
    if (!layout || !layout->preeditAreaText().isEmpty()) continue;
    const auto origin = document_->documentLayout()->blockBoundingRect(block).topLeft();
    for (int n = 0; n < layout->lineCount(); ++n) {
      const auto line = layout->lineAt(n);
      const int left = std::max(from, line.textStart()), right = std::min(to, line.textStart() + line.textLength());
      const qreal y = origin.y() + line.y() + line.ascent() + 2;
      if (left >= right || y < top || y > bottom) continue;
      const qreal x1 = origin.x() + line.cursorToX(left), x2 = origin.x() + line.cursorToX(right);
      result.append(QVariantMap{{"x", std::min(x1, x2)}, {"y", y}, {"width", std::abs(x2 - x1)}});
    }
  }
  return result;
}
}
