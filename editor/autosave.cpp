#include "editor/autosave.h"
#include "editor/screenplay_layout.h"
#include "source/fountain_working.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QTextCursor>
#include <QWindow>
#include <algorithm>

namespace apollo::editor {
Autosaver::Autosaver(QTextDocument* document, std::unique_ptr<source::ProjectSession> session, QObject* parent)
    : QObject(parent), document_(document), session_(std::move(session)), location_(session_->opened().project.directory),
      watched_source_(session_->opened().project.screenplay_path()), project_id_(session_->opened().project.id), revision_(session_->opened().screenplay.revision),
      captured_(revision_), saved_(revision_) {
  if (session_->opened().project.import_review)
    throw source::SourceError(source::SourceError::Kind::malformed, QStringLiteral("Fountain intake is read-only; screenplay conversion is pending"));
  if (session_->opened().project.import_working) document->setProperty(export_block_property, source::imported_pdf_block);
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
  } else if (document_ && reload_busy_) {
    std::lock_guard lock(mutex_);
    pending_ = Snapshot{revision_, document_elements(*document_), false, true};
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
  return saved_ == revision_ && !copy_busy_ && !reload_busy_ ? QStringLiteral("saved") : QStringLiteral("saving");
}

QString Autosaver::status() const {
  if (error_.isEmpty()) {
    if (state() != QStringLiteral("saved")) return QStringLiteral("Saving…");
    return recovery_warning_.isEmpty() ? QStringLiteral("Saved") : QStringLiteral("Saved — recovery snapshot failed: %1").arg(recovery_warning_);
  }
  auto reason = error_.left(1).toUpper() + error_.mid(1);
  if (!reason.endsWith('.')) reason += '.';
  return QStringLiteral("Not saved to project — %1 %2").arg(reason, recovery_status());
}

QString Autosaver::recovery_status() const {
  if (reload_busy_) return QStringLiteral("Preserving writing and reloading…");
  QString result;
  if (current_protected()) result = QStringLiteral("Current writing protected by emergency copy at %1 (%2).")
      .arg(recovery_path_, recovery_time_.toLocalTime().toString(Qt::ISODateWithMs));
  else if (protected_revision_) result = QStringLiteral("Emergency copy at %1 protects an earlier revision; latest writing is unprotected.").arg(recovery_path_);
  else result = QStringLiteral("Latest writing has no verified emergency copy.");
  if (!recovery_error_.isEmpty()) result += QStringLiteral(" Emergency copy failed: %1").arg(recovery_error_);
  if (!recovery_warning_.isEmpty()) result += QStringLiteral(" Recovery snapshots: %1").arg(recovery_warning_);
  return result;
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
  if (leaving_) return;
  {
    std::lock_guard lock(mutex_);
    verify_requested_ = true;
  }
  wake_.notify_one();
}

void Autosaver::changed() {
  if (loading_) return;
  const auto before = status();
  ++revision_;
  if (!timer_.isActive()) timer_.start();
  if (status() != before) emit statusChanged();
  emit recoveryChanged();
}

void Autosaver::retry() {
  if (close_dialog_) close_requested_ = true;
  capture(true);
}

bool Autosaver::save_copy(const QString& destination) {
  if (leaving_ || copy_busy_ || reload_busy_ || !document_) return false;
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

void Autosaver::prepare_leave(bool retry_save) {
  if (leaving_ || copy_busy_ || reload_busy_ || !document_) return;
  leaving_ = true;
  verification_timer_.stop();
  capture(retry_save);
  {
    std::lock_guard lock(mutex_);
    leave_request_ = revision_;
  }
  wake_.notify_one();
}

void Autosaver::release_for_leave() {
  if (!leaving_ || releasing_) return;
  if (state() != QStringLiteral("saved")) {
    leaving_ = false;
    prepare_leave();
    return;
  }
  releasing_ = true;
  {
    std::lock_guard lock(mutex_);
    release_request_ = revision_;
  }
  wake_.notify_one();
}

void Autosaver::capture(bool force) {
  timer_.stop();
  if (!document_ || copy_busy_ || reload_busy_ || (!force && captured_ == revision_)) return;
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

bool Autosaver::reload() {
  if (leaving_ || copy_busy_ || reload_busy_ || !document_) return false;
  timer_.stop();
  reload_busy_ = true;
  reload_status_ = QStringLiteral("Preserving writing and reloading…");
  close_requested_ = false;
  {
    std::lock_guard lock(mutex_);
    pending_.reset();
    reload_request_ = ReloadRequest{{revision_, document_elements(*document_), false}, std::nullopt};
  }
  wake_.notify_one();
  emit recoveryChanged();
  emit statusChanged();
  return true;
}

void Autosaver::protect_current() {
  if (!document_ || copy_busy_ || reload_busy_) return;
  timer_.stop();
  captured_ = revision_;
  {
    std::lock_guard lock(mutex_);
    pending_ = Snapshot{revision_, document_elements(*document_), false, true};
  }
  wake_.notify_one();
}

void Autosaver::recovery_finished(std::uint64_t revision, const std::optional<source::RecoveryCopy>& copy, const QString& error) {
  if (copy) {
    protected_revision_ = revision;
    recovery_path_ = copy->directory;
    recovery_time_ = copy->captured_at;
  }
  recovery_error_ = error;
  emit recoveryChanged();
  emit statusChanged();
}

void Autosaver::reload_finished(std::uint64_t revision, const std::optional<source::OpenedProject>& candidate, const QString& error) {
  reload_busy_ = false;
  const bool success = candidate && error.isEmpty() && revision == revision_;
  if (success) {
    loading_ = true;
    document_->setUndoRedoEnabled(false);
    QTextCursor cursor(document_);
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    cursor.removeSelectedText();
    insert_elements(cursor, candidate->screenplay.elements);
    cursor.endEditBlock();
    document_->setUndoRedoEnabled(true);
    document_->setModified(false);
    loading_ = false;
    revision_ = saved_ = captured_ = candidate->screenplay.revision;
    protected_revision_.reset(); // The old copy covers the pre-Reload document.
    watched_source_ = candidate->project.screenplay_path();
    error_.clear();
    paused_ = false;
    reload_cancelled_ = false;
    reload_status_ = QStringLiteral("Reloaded project. Previous writing preserved at %1 (%2).")
        .arg(recovery_path_, recovery_time_.toLocalTime().toString(Qt::ISODateWithMs));
    copy_error_.clear();
    copy_status_.clear();
    emit copyChanged();
  } else {
    error_ = error.isEmpty() ? QStringLiteral("Reload cancelled because writing changed. Preserve and Reload again or Save a Copy.") : error;
    paused_ = true;
    // No Retry may write memory over a baseline accepted during a cancelled Reload.
    if (candidate) reload_cancelled_ = true;
    reload_status_ = QStringLiteral("Reload failed — %1").arg(error_);
    // A timer may have expired while Reload was busy. Keep later writing
    // protected even when there is no next keystroke.
    if (revision_ != revision) protect_current();
  }
  watch_destination();
  emit recoveryChanged();
  emit statusChanged();
  emit reloadCompleted(success);
  if (close_requested_ && closing_) closing_->close();
}

void Autosaver::write_loop() {
  std::optional<source::Screenplay> initial = session_->opened().screenplay;
  auto initial_time = QDateTime::currentDateTimeUtc();
  auto preserve = [&](const Snapshot& snapshot, const QString& reason) {
    std::optional<source::RecoveryCopy> copy;
    QString error;
    try {
      if (hook_) hook_(Phase::recovering, snapshot.revision);
      source::RecoveryStore store(session_->opened().project, session_->opened().assets,
                                  source::RecoveryStore::emergency_directory(session_->opened().project));
      copy = store.preserve({snapshot.revision, snapshot.elements}, reason, snapshot.captured_at);
      if (hook_) hook_(Phase::recovered, snapshot.revision);
    } catch (const std::exception& failure) { error = QString::fromUtf8(failure.what()); }
    QMetaObject::invokeMethod(this, [this, revision = snapshot.revision, copy, error] { recovery_finished(revision, copy, error); }, Qt::QueuedConnection);
    return std::make_pair(copy, error);
  };
  auto local_recovery = [&](const source::Screenplay& screenplay, const QDateTime& captured_at) {
    QString error;
    try {
      session_->check_destination();
      if (hook_) hook_(Phase::snapshotting, screenplay.revision);
      source::RecoveryStore store(session_->opened().project, session_->opened().assets, session_->opened().project.directory + "/.recovery");
      store.preserve(screenplay, QStringLiteral("autosave"), captured_at);
      if (hook_) hook_(Phase::snapshotted, screenplay.revision);
    } catch (const std::exception& failure) { error = QString::fromUtf8(failure.what()); }
    QMetaObject::invokeMethod(this, [this, error] { recovery_warning_ = error; emit recoveryChanged(); emit statusChanged(); }, Qt::QueuedConnection);
  };
  for (;;) {
    std::optional<Snapshot> snapshot;
    std::optional<ReloadRequest> reload;
    std::optional<CopyRequest> copy;
    std::optional<std::uint64_t> leave, release;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this] { return pending_ || copy_request_ || reload_request_ || verify_requested_ || leave_request_ || release_request_ || stopping_; });
      if (!pending_ && !copy_request_ && !reload_request_ && stopping_) return;
      if (!pending_ && !copy_request_ && !reload_request_) {
        leave = leave_request_;
        leave_request_.reset();
        release = release_request_;
        release_request_.reset();
      }
      copy = std::move(copy_request_);
      copy_request_.reset();
      reload = std::move(reload_request_);
      reload_request_.reset();
      if (!copy && !reload) {
        snapshot = std::move(pending_);
        pending_.reset();
      }
      verify_requested_ = false;
    }
    if (release) {
      QString failure;
      try { session_->check_destination(); }
      catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
      if (failure.isEmpty()) {
        // QLockFile removes the filesystem lock in its destructor. Do that
        // here, not later in the GUI-thread Autosaver destructor.
        session_.reset();
        QMetaObject::invokeMethod(this, [this] { emit released(); }, Qt::QueuedConnection);
        return;
      }
      QMetaObject::invokeMethod(this, [this, failure] {
        releasing_ = leaving_ = false;
        error_ = failure;
        verification_timer_.start();
        emit statusChanged();
        emit leaveReady(false);
      }, Qt::QueuedConnection);
      continue;
    }
    if (leave) {
      QString failure;
      try { session_->check_destination(); }
      catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
      QMetaObject::invokeMethod(this, [this, revision = *leave, failure] {
        if (revision != revision_) {
          leaving_ = false;
          prepare_leave();
          return;
        }
        if (!failure.isEmpty()) { error_ = failure; emit statusChanged(); }
        const bool ready = failure.isEmpty() && state() == QStringLiteral("saved");
        if (!ready) {
          leaving_ = false;
          verification_timer_.start();
        }
        emit leaveReady(ready);
      }, Qt::QueuedConnection);
      continue;
    }
    if (reload) {
      QString error;
      std::optional<source::OpenedProject> candidate;
      try {
        if (!reload->candidate) {
          const auto protection = preserve(reload->snapshot, QStringLiteral("reload"));
          if (!protection.first) throw std::runtime_error(protection.second.toStdString());
          if (hook_) hook_(Phase::reloading, reload->snapshot.revision);
          candidate = session_->read_reload();
          QMetaObject::invokeMethod(this, [this, request = *reload, candidate]() mutable {
            if (revision_ != request.snapshot.revision) {
              reload_finished(request.snapshot.revision, std::nullopt, QStringLiteral("Reload cancelled because writing changed; current writing remains in this window."));
              return;
            }
            request.candidate = candidate;
            { std::lock_guard lock(mutex_); reload_request_ = std::move(request); }
            wake_.notify_one();
          }, Qt::QueuedConnection);
          continue;
        }
        candidate = reload->candidate;
        session_->accept_reload(*candidate);
        initial = candidate->screenplay;
        initial_time = QDateTime::currentDateTimeUtc();
      } catch (const std::exception& failure) { error = QString::fromUtf8(failure.what()); candidate.reset(); }
      QMetaObject::invokeMethod(this, [this, revision = reload->snapshot.revision, candidate, error] { reload_finished(revision, candidate, error); }, Qt::QueuedConnection);
      continue;
    }
    if (copy) {
      QString error, destination, project_id;
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
        project_id = replacement->opened().project.id;
        session_ = std::move(replacement);
        paused_ = false;
        reload_cancelled_ = false;
        initial = session_->opened().screenplay;
        initial_time = QDateTime::currentDateTimeUtc();
        {
          std::lock_guard lock(mutex_);
          if (pending_) pending_->revision -= copy->snapshot.revision;
          copy_base_ = copy->snapshot.revision;
        }
      } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        if (!destination.isEmpty()) error += QStringLiteral(" A complete copy was created at %1 but could not be activated.").arg(destination);
      }
      QMetaObject::invokeMethod(this, [this, revision = copy->snapshot.revision, destination, project_id, error] {
        copy_finished(revision, destination, project_id, error);
      }, Qt::QueuedConnection);
      continue;
    }
    if (snapshot && (snapshot->emergency_only || reload_cancelled_ || (paused_ && !snapshot->retry))) {
      preserve(*snapshot, QStringLiteral("emergency"));
      continue;
    }
    if (paused_ && !snapshot) continue;
    const auto revision = snapshot ? std::optional(snapshot->revision) : std::nullopt;
    QString error;
    try {
      if (snapshot) {
        if (hook_) hook_(Phase::writing, *revision);
        const auto bytes = source::serialize_screenplay({*revision, snapshot->elements});
        if (initial) { local_recovery(*initial, initial_time); initial.reset(); }
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
    // Report the failed project destination immediately; slow emergency I/O
    // must not leave the GUI claiming that the original is still saving.
    if (snapshot && !error.isEmpty()) preserve(*snapshot, QStringLiteral("emergency"));
    if (snapshot && error.isEmpty()) local_recovery({snapshot->revision, snapshot->elements}, snapshot->captured_at);
  }
}

void Autosaver::finished(std::optional<std::uint64_t> revision, const QString& error) {
  const auto before = status();
  if (error.isEmpty()) {
    // Writes finish in capture order, so a success covers every earlier failure.
    saved_ = std::max(saved_, *revision);
    error_.clear();
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
    else if (state() == QStringLiteral("failed")) {
      close_dialog_ = true;
      close_requested_ = false;
      emit recoveryChanged();
    }
  }
}

void Autosaver::copy_finished(std::uint64_t revision, const QString& destination, const QString& project_id, const QString& error) {
  copy_busy_ = false;
  copy_error_ = error;
  if (error.isEmpty()) {
    location_ = destination;
    project_id_ = project_id;
    watched_source_ = destination + "/screenplay.json";
    // Changes made after the snapshot belong to the new project's sequence.
    revision_ -= revision;
    saved_ = captured_ = 0;
    error_.clear();
    protected_revision_.reset();
    recovery_path_.clear();
    reload_status_.clear();
    recovery_error_.clear();
    recovery_warning_.clear();
    close_dialog_ = false;
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
  emit recoveryChanged();
  emit statusChanged();
  emit copyCompleted(error.isEmpty());
  if (close_requested_ && closing_ && state() == QStringLiteral("saved")) closing_->close();
}

void Autosaver::guard_close(QWindow& window) {
  closing_ = &window;
  window.installEventFilter(this);
}

void Autosaver::keep_open() {
  close_dialog_ = false;
  close_requested_ = false;
  emit recoveryChanged();
}

void Autosaver::close_with_copy_or_discard() {
  if (!closing_ || copy_busy_ || reload_busy_) return;
  allow_close_ = true;
  closing_->close();
}

bool Autosaver::eventFilter(QObject* watched, QEvent* event) {
  if (watched == closing_ && event->type() == QEvent::Close && leaving_) {
    event->ignore();
    return true;
  }
  if (watched == closing_ && event->type() == QEvent::Close && (copy_busy_ || reload_busy_)) {
    event->ignore();
    close_requested_ = true;
    return true;
  }
  if (watched != closing_ || event->type() != QEvent::Close || state() == QStringLiteral("saved") || allow_close_)
    return QObject::eventFilter(watched, event);
  event->ignore();
  if (state() == QStringLiteral("failed")) {
    close_dialog_ = true;
    protect_current();
    emit recoveryChanged();
  } else {
    close_requested_ = true;
    flush();
  }
  return true;
}
}
