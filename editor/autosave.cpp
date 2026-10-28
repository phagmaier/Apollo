#include "editor/autosave.h"
#include "editor/screenplay_layout.h"

#include <QCloseEvent>
#include <QWindow>
#include <algorithm>

namespace apollo::editor {
Autosaver::Autosaver(QTextDocument* document, source::Project project, std::uint64_t revision, QObject* parent)
    : QObject(parent), document_(document), project_(std::move(project)), revision_(revision), captured_(revision), saved_(revision) {
  timer_.setSingleShot(true);
  // Measured from the first unsaved change, not restarted by later ones, so
  // continuous typing cannot postpone a capture.
  timer_.setInterval(capture_interval_ms);
  connect(&timer_, &QTimer::timeout, this, [this] { capture(false); });
  connect(document, &QTextDocument::contentsChange, this, [this](int, int, int) { changed(); });
  writer_ = std::thread([this] { write_loop(); });
}

Autosaver::~Autosaver() {
  if (document_ && captured_ != revision_) capture(false);
  {
    std::lock_guard lock(mutex_);
    stopping_ = true;
  }
  wake_.notify_one();
  writer_.join();
}

QString Autosaver::state() const {
  if (!error_.isEmpty()) return QStringLiteral("failed");
  return saved_ == revision_ ? QStringLiteral("saved") : QStringLiteral("saving");
}

QString Autosaver::status() const {
  if (error_.isEmpty()) return saved_ == revision_ ? QStringLiteral("Saved") : QStringLiteral("Saving…");
  return QStringLiteral("Not saved — your latest writing is only in this window. %1%2.%3")
      .arg(error_.left(1).toUpper(), error_.mid(1), discard_armed_ ? QStringLiteral(" Close again to discard it.") : QString());
}

void Autosaver::changed() {
  const auto before = state();
  ++revision_;
  if (!timer_.isActive()) timer_.start();
  if (state() != before) emit statusChanged();
}

void Autosaver::retry() { capture(true); }

void Autosaver::flush() { capture(false); }

void Autosaver::capture(bool force) {
  timer_.stop();
  if (!document_ || (!force && captured_ == revision_)) return;
  if (hook_) hook_(Phase::capturing, revision_);
  Snapshot snapshot{revision_, document_elements(*document_)};
  captured_ = revision_;
  timer_.stop();
  if (hook_) hook_(Phase::captured, captured_);
  {
    std::lock_guard lock(mutex_);
    // An older snapshot still waiting is superseded, never written after this.
    pending_ = std::move(snapshot);
  }
  wake_.notify_one();
}

void Autosaver::write_loop() {
  for (;;) {
    Snapshot snapshot;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this] { return pending_ || stopping_; });
      if (!pending_) return;
      snapshot = std::move(*pending_);
      pending_.reset();
    }
    const auto revision = snapshot.revision;
    QString error;
    try {
      if (hook_) hook_(Phase::writing, revision);
      const auto bytes = source::serialize_screenplay({revision, std::move(snapshot.elements)});
      source::replace_file(project_.screenplay_path(), bytes, [&](source::WritePhase phase) {
        if (hook_ && phase == source::WritePhase::written) hook_(Phase::written, revision);
        if (hook_ && phase == source::WritePhase::committed) hook_(Phase::committed, revision);
      });
    } catch (const std::exception& failure) {
      error = QString::fromUtf8(failure.what());
    }
    QMetaObject::invokeMethod(this, [this, revision, error] { finished(revision, error); }, Qt::QueuedConnection);
  }
}

void Autosaver::finished(std::uint64_t revision, const QString& error) {
  const auto before = status();
  if (error.isEmpty()) {
    // Writes finish in capture order, so a success covers every earlier failure.
    saved_ = std::max(saved_, revision);
    error_.clear();
    discard_armed_ = false;
    if (hook_) hook_(Phase::acknowledged, revision);
  } else {
    error_ = error;
  }
  if (status() != before) emit statusChanged();
  if (close_requested_ && closing_) {
    if (state() == QStringLiteral("saved")) closing_->close();
    else if (state() == QStringLiteral("failed")) close_requested_ = false;
  }
}

void Autosaver::guard_close(QWindow& window) {
  closing_ = &window;
  window.installEventFilter(this);
}

bool Autosaver::eventFilter(QObject* watched, QEvent* event) {
  if (watched != closing_ || event->type() != QEvent::Close || state() == QStringLiteral("saved") || discard_armed_)
    return QObject::eventFilter(watched, event);
  event->ignore();
  if (state() == QStringLiteral("failed")) {
    discard_armed_ = true;
    emit statusChanged();
  } else {
    close_requested_ = true;
    flush();
  }
  return true;
}
}
