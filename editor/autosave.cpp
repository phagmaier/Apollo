#include "editor/autosave.h"
#include "editor/screenplay_layout.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QWindow>
#include <algorithm>

namespace apollo::editor {
Autosaver::Autosaver(QTextDocument* document, std::unique_ptr<source::ProjectSession> session, QObject* parent)
    : QObject(parent), document_(document), session_(std::move(session)), location_(session_->opened().project.directory),
      watched_source_(session_->opened().project.screenplay_path()), revision_(session_->opened().screenplay.revision),
      captured_(revision_), saved_(revision_) {
  timer_.setSingleShot(true);
  // Measured from the first unsaved change, not restarted by later ones, so
  // continuous typing cannot postpone a capture.
  timer_.setInterval(capture_interval_ms);
  connect(&timer_, &QTimer::timeout, this, [this] { capture(false); });
  connect(document, &QTextDocument::contentsChange, this, [this](int, int, int) { changed(); });
  connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] { request_verification(); });
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { request_verification(); });
  // Also poll: watches can be lost on replacement/unmount or be unavailable.
  verification_timer_.setInterval(1000);
  connect(&verification_timer_, &QTimer::timeout, this, [this] { watch_destination(); request_verification(); });
  watch_destination();
  verification_timer_.start();
  writer_ = std::thread([this] { write_loop(); });
  request_verification();
}

Autosaver::~Autosaver() {
  if (document_ && copy_busy_) {
    auto elements = document_elements(*document_);
    std::lock_guard lock(mutex_);
    pending_ = Snapshot{revision_ - copy_base_.value_or(0), std::move(elements), false};
  } else if (document_ && captured_ != revision_) capture(false);
  {
    std::lock_guard lock(mutex_);
    stopping_ = true;
  }
  wake_.notify_one();
  writer_.join();
}

QString Autosaver::state() const {
  if (!error_.isEmpty()) return QStringLiteral("failed");
  return saved_ == revision_ && !copy_busy_ ? QStringLiteral("saved") : QStringLiteral("saving");
}

QString Autosaver::status() const {
  if (error_.isEmpty()) return state() == QStringLiteral("saved") ? QStringLiteral("Saved") : QStringLiteral("Saving…");
  auto reason = error_.left(1).toUpper() + error_.mid(1);
  if (!reason.endsWith('.')) reason += '.';
  return QStringLiteral("Not saved — your latest writing is only in this window. %1%2")
      .arg(reason, discard_armed_ ? QStringLiteral(" Close again to discard it.") : QString());
}

void Autosaver::watch_destination() {
  // The manifest supports a custom screenplay name. Keep the GUI independent
  // of session_ while the worker switches it to a copy.
  const QStringList paths{location_, location_ + "/project.json", watched_source_};
  const auto watched = watcher_.files() + watcher_.directories();
  for (const auto& path : paths)
    if (!watched.contains(path) && QFileInfo::exists(path)) watcher_.addPath(path);
}

void Autosaver::request_verification() {
  {
    std::lock_guard lock(mutex_);
    verify_requested_ = true;
  }
  wake_.notify_one();
}

void Autosaver::changed() {
  const auto before = state();
  ++revision_;
  if (!timer_.isActive()) timer_.start();
  if (state() != before) emit statusChanged();
}

void Autosaver::retry() { capture(true); }

bool Autosaver::save_copy(const QString& destination) {
  if (copy_busy_ || !document_) return false;
  copy_error_.clear();
  if (destination.isEmpty() || !QFileInfo(destination).isAbsolute()) {
    copy_error_ = QStringLiteral("Choose an absolute path for the new project folder.");
    copy_status_ = copy_error_;
    emit copyChanged();
    return false;
  }
  timer_.stop();
  const auto path = QDir::cleanPath(destination);
  CopyRequest request{path, {revision_, document_elements(*document_), false}};
  copy_busy_ = true;
  close_requested_ = false;
  discard_armed_ = false;
  copy_status_ = QStringLiteral("Saving a copy to %1…").arg(path);
  {
    std::lock_guard lock(mutex_);
    // An in-flight save finishes first. Queued old-destination writes are
    // superseded by this complete snapshot; new edits wait for activation.
    pending_.reset();
    copy_base_.reset();
    copy_request_ = std::move(request);
  }
  wake_.notify_one();
  emit copyChanged();
  emit statusChanged();
  return true;
}

void Autosaver::flush() { capture(false); }

void Autosaver::capture(bool force) {
  timer_.stop();
  if (!document_ || copy_busy_ || (!force && (captured_ == revision_ || paused_))) return;
  if (hook_) hook_(Phase::capturing, revision_);
  Snapshot snapshot{revision_, document_elements(*document_), force};
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
    std::optional<Snapshot> snapshot;
    std::optional<CopyRequest> copy;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this] { return pending_ || copy_request_ || verify_requested_ || stopping_; });
      if (!pending_ && !copy_request_ && stopping_) return;
      copy = std::move(copy_request_);
      copy_request_.reset();
      if (!copy) {
        snapshot = std::move(pending_);
        pending_.reset();
      }
      verify_requested_ = false;
    }
    if (copy) {
      QString error, destination;
      try {
        if (hook_) hook_(Phase::copying, copy->snapshot.revision);
        // A new project has its own revision sequence. Keeping element ids
        // and the live QTextDocument preserves exact writing and undo.
        const source::Screenplay screenplay{0, std::move(copy->snapshot.elements)};
        const auto project = source::create_project(copy->destination, screenplay, session_->opened().assets);
        destination = project.directory;
        if (hook_) hook_(Phase::copied, copy->snapshot.revision);
        auto replacement = std::make_unique<source::ProjectSession>(destination);
        replacement->check_destination();
        if (replacement->opened().screenplay != source::canonical(screenplay) || replacement->opened().assets != session_->opened().assets)
          throw source::SourceError(source::SourceError::Kind::external_change, QStringLiteral("the new project changed before activation"));
        destination = replacement->opened().project.directory;
        session_ = std::move(replacement);
        paused_ = false;
        {
          std::lock_guard lock(mutex_);
          if (pending_) pending_->revision -= copy->snapshot.revision;
          copy_base_ = copy->snapshot.revision;
        }
      } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        if (!destination.isEmpty()) error += QStringLiteral(" A complete copy was created at %1 but could not be activated.").arg(destination);
      }
      QMetaObject::invokeMethod(this, [this, revision = copy->snapshot.revision, destination, error] {
        copy_finished(revision, destination, error);
      }, Qt::QueuedConnection);
      continue;
    }
    if (paused_ && (!snapshot || !snapshot->retry)) continue;
    const auto revision = snapshot ? std::optional(snapshot->revision) : std::nullopt;
    QString error;
    try {
      if (snapshot) {
        if (hook_) hook_(Phase::writing, *revision);
        const auto bytes = source::serialize_screenplay({*revision, std::move(snapshot->elements)});
        session_->save(bytes, [&](source::WritePhase phase) {
          if (hook_ && phase == source::WritePhase::written) hook_(Phase::written, *revision);
          if (hook_ && phase == source::WritePhase::committed) hook_(Phase::committed, *revision);
        });
        paused_ = false;
      } else session_->check_destination();
    } catch (const source::SourceError& failure) {
      if (failure.kind() == source::SourceError::Kind::external_change) paused_ = true;
      error = QString::fromUtf8(failure.what());
    } catch (const std::exception& failure) {
      error = QString::fromUtf8(failure.what());
    }
    if (snapshot || !error.isEmpty())
      QMetaObject::invokeMethod(this, [this, revision, error] { finished(revision, error); }, Qt::QueuedConnection);
  }
}

void Autosaver::finished(std::optional<std::uint64_t> revision, const QString& error) {
  const auto before = status();
  if (error.isEmpty()) {
    // Writes finish in capture order, so a success covers every earlier failure.
    saved_ = std::max(saved_, *revision);
    error_.clear();
    discard_armed_ = false;
    if (hook_) hook_(Phase::acknowledged, *revision);
    // A capture timer may have expired while an explicit retry was still
    // paused. Resume protection of writing made during that retry immediately.
    if (captured_ != revision_ && !timer_.isActive()) capture(false);
  } else {
    error_ = error;
  }
  watch_destination();
  if (status() != before) emit statusChanged();
  if (!copy_busy_ && close_requested_ && closing_) {
    if (state() == QStringLiteral("saved")) closing_->close();
    else if (state() == QStringLiteral("failed")) close_requested_ = false;
  }
}

void Autosaver::copy_finished(std::uint64_t revision, const QString& destination, const QString& error) {
  copy_busy_ = false;
  copy_error_ = error;
  if (error.isEmpty()) {
    location_ = destination;
    watched_source_ = destination + "/screenplay.json";
    // Changes made after the snapshot belong to the new project's sequence.
    revision_ -= revision;
    saved_ = captured_ = 0;
    error_.clear();
    discard_armed_ = false;
    copy_status_ = QStringLiteral("Copy active — %1").arg(destination);
    const auto paths = watcher_.files() + watcher_.directories();
    if (!paths.isEmpty()) watcher_.removePaths(paths);
    emit locationChanged();
  } else {
    copy_status_ = QStringLiteral("Copy failed — %1 Current project: %2").arg(error, location_);
    captured_ = saved_; // Requeue any superseded writing at the old destination.
    close_requested_ = false;
  }
  watch_destination();
  capture(false);
  emit copyChanged();
  emit statusChanged();
  emit copyCompleted(error.isEmpty());
  if (close_requested_ && closing_ && state() == QStringLiteral("saved")) closing_->close();
}

void Autosaver::guard_close(QWindow& window) {
  closing_ = &window;
  window.installEventFilter(this);
}

bool Autosaver::eventFilter(QObject* watched, QEvent* event) {
  if (watched == closing_ && event->type() == QEvent::Close && copy_busy_) {
    event->ignore();
    close_requested_ = true;
    return true;
  }
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
