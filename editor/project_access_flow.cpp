#include "editor/project_access_flow.h"
#include "source/fountain_conversion.h"
#include "editor/autosave.h"
#include "editor/editor_view.h"
#include "editor/pdf_export_job.h"
#include "editor/screenplay_layout.h"
#include "source/project_creation.h"

#include <QDir>
#include <QFileInfo>
#include <QQuickItem>
#include <QQuickView>
#include <QTextDocument>

namespace apollo::editor {
ProjectAccessFlow::ProjectAccessFlow(QQuickView& view, QFont font, QString font_directory, QString exporter, QString session_path)
    : view_(view), font_(std::move(font)), font_directory_(std::move(font_directory)), exporter_(std::move(exporter)),
      state_(session_path), library_(session_path.isEmpty() ? QString() : session_path + ".recents") {
  state_timer_.setSingleShot(true);
  state_timer_.setInterval(250);
  connect(&state_timer_, &QTimer::timeout, this, &ProjectAccessFlow::persist_session);
}
QQuickItem* ProjectAccessFlow::editor() const { return editor_; }

ProjectAccessFlow::~ProjectAccessFlow() {
  state_timer_.stop();
  if (worker_.joinable()) worker_.join();
  if (saver_ && document_ && editor_) {
    try { state_.save(SessionState::capture(saver_->location(), saver_->project_id(), *document_, editor_->property("cursorPosition").toInt())); }
    catch (const std::exception& error) { qWarning("Session resume not saved: %s", error.what()); }
  }
  // Destroy the TextEdit before the document/commands it references.
  view_.setSource({});
  export_job_.reset();
  saver_.reset();
}

QVariantList ProjectAccessFlow::recent_projects() const {
  QVariantList result;
  for (const auto& recent : recents_)
    result.push_back(QVariantMap{{"name", QFileInfo(recent.directory).fileName()}, {"location", recent.directory}});
  return result;
}
QUrl ProjectAccessFlow::folder_url(const QString& path) const { return QUrl::fromLocalFile(path); }
QString ProjectAccessFlow::local_folder(const QUrl& url) const { return url.isLocalFile() ? url.toLocalFile() : QString(); }

void ProjectAccessFlow::show_access() {
  view_.setResizeMode(QQuickView::SizeRootObjectToView);
  view_.setInitialProperties({{"access", QVariant::fromValue<QObject*>(this)}});
  view_.setSource(QUrl::fromLocalFile(QStringLiteral(APOLLO_ACCESS_QML)));
  if (view_.status() != QQuickView::Ready) throw std::runtime_error("cannot load project access QML");
}

void ProjectAccessFlow::start(const QString& directory, const QString& fountain) {
  show_access();
  busy_ = true;
  emit changed();
  launch([this, directory, fountain] {
    std::optional<ResumeState> resume;
    std::vector<RecentProject> recents;
    QString session_error, library_error;
    try { resume = state_.load(); }
    catch (const std::exception& error) { session_error = QStringLiteral("Session resume unavailable: %1").arg(QString::fromUtf8(error.what())); }
    try { recents = library_.load(); }
    catch (const std::exception& error) { library_error = QStringLiteral("Recent projects unavailable: %1").arg(QString::fromUtf8(error.what())); }
    const auto default_location = source::default_project_location();
    QMetaObject::invokeMethod(this, [this, directory, fountain, resume = std::move(resume), recents = std::move(recents), session_error, library_error, default_location]() mutable {
      busy_ = false;
      resume_ = std::move(resume);
      recents_ = std::move(recents);
      default_location_ = default_location;
      session_notice_ = session_error;
      library_notice_ = library_error;
      update_notice();
      if (!fountain.isEmpty()) preview_import(fountain);
      else if (!directory.isEmpty()) inspect(directory, {});
      else if (resume_) inspect(resume_->directory, resume_->project_id);
    }, Qt::QueuedConnection);
  });
}

void ProjectAccessFlow::open_project(const QString& directory) {
  if (busy_ || document_ || review_session_ || intake_) return;
  if (directory.trimmed().isEmpty()) {
    error_ = QStringLiteral("Choose an existing Apollo project folder.");
    emit changed();
    return;
  }
  inspect(directory, {});
}
void ProjectAccessFlow::open_recent(int index) {
  if (busy_ || document_ || review_session_ || intake_ || index < 0 || static_cast<std::size_t>(index) >= recents_.size()) return;
  const auto& recent = recents_[static_cast<std::size_t>(index)];
  inspect(recent.directory, recent.project_id);
}

void ProjectAccessFlow::create_project(const QString& parent, const QString& name) {
  if (busy_ || document_ || review_session_ || intake_) return;
  busy_ = true;
  error_.clear(); candidates_.clear(); current_available_ = false; current_summary_.clear();
  emit changed();
  launch([this, parent, name] {
    access_.reset();
    std::optional<source::Project> project;
    QString failure;
    try { project = source::create_blank_project(parent, name); }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
    QMetaObject::invokeMethod(this, [this, project, failure] {
      busy_ = false;
      if (!project) { error_ = failure; emit changed(); return; }
      // Even our freshly published projects acquire safe edit access and check
      // the expected UUID; a competing open or changed project is not bypassed.
      inspect(project->directory, project->id);
    }, Qt::QueuedConnection);
  });
}

void ProjectAccessFlow::show_intake() {
  view_.setInitialProperties({{"access", QVariant::fromValue<QObject*>(this)}});
  view_.setSource(QUrl::fromLocalFile(QStringLiteral(APOLLO_INTAKE_QML)));
  if (view_.status() != QQuickView::Ready) throw std::runtime_error("cannot load imported material QML");
}

void ProjectAccessFlow::preview_import(const QString& path) {
  if (busy_ || document_ || review_session_) return;
  busy_ = true; error_.clear(); emit changed();
  launch([this, path] {
    access_.reset();
    std::optional<source::FountainIntake> intake;
    QString failure;
    try { intake = source::read_fountain_intake(path); }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
    QMetaObject::invokeMethod(this, [this, intake = std::move(intake), failure]() mutable {
      busy_ = false;
      error_ = failure;
      if (intake) { intake_ = std::move(intake); location_.clear(); candidates_.clear(); current_available_ = false; show_intake(); }
      emit changed();
    }, Qt::QueuedConnection);
  });
}

void ProjectAccessFlow::publish_import(const QString& parent, const QString& name) {
  if (review_session_) return;
  publish_import_impl(parent, name, false);
}
bool ProjectAccessFlow::intake_interpreted() const {
  return intake_ && intake_->interpreted;
}
void ProjectAccessFlow::interpret_import(const QString& parent, const QString& name) {
  publish_import_impl(parent, name, true);
}
void ProjectAccessFlow::publish_import_impl(const QString& parent, const QString& name, bool interpret) {
  if (busy_ || document_ || !intake_) return;
  // Move ownership on the GUI thread; QML never observes a pointer being reset
  // by the worker. Failure returns the original locked reader unchanged.
  publishing_reader_ = review_session_ != nullptr;
  auto reader = std::move(review_session_);
  busy_ = true; error_.clear(); emit changed();
  launch([this, parent, name, interpret, assets = intake_->assets, reader = std::move(reader)]() mutable {
    std::optional<source::Project> project;
    QString failure;
    try {
      if (reader) {
        reader->check_destination();
        if (source::open_project(reader->opened().project.directory).assets != assets)
          throw source::SourceError(source::SourceError::Kind::external_change, "Retained source changed; reopen it before making an interpretation copy.");
      }
      if (interpret) assets = source::interpreted_fountain_assets(assets);
      project = source::create_named_project(parent, name, {0, {{ElementType::action, ""}}}, assets);
      reader.reset(); // Release the old reader only after publication.
    }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
    QMetaObject::invokeMethod(this, [this, project, failure, assets = std::move(assets), reader = std::move(reader)]() mutable {
      review_session_ = std::move(reader);
      publishing_reader_ = false;
      busy_ = false;
      if (!project) { error_ = failure; emit changed(); return; }
      intake_.reset();
      show_access();
      // Activation can still fail (changed/locked project). Retain the
      // published folder and its path through the existing safe-access UI.
      inspect(project->directory, project->id, assets);
    }, Qt::QueuedConnection);
  });
}

void ProjectAccessFlow::install_intake(std::unique_ptr<source::ProjectSession> session, source::FountainIntake intake) {
  const auto project = session->opened().project;
  review_session_ = std::move(session);
  intake_ = std::move(intake);
  location_ = project.directory;
  show_intake();
  // There is no screenplay cursor yet. Persist only the active project and
  // identity, using the same validated session format and recent-list merge.
  busy_ = true; emit changed();
  launch([this, project] {
    QString failure, library_error;
    std::optional<std::vector<RecentProject>> recents;
    const ResumeState snapshot{project.directory, project.id, {}, 0, 0};
    try { state_.save(snapshot); }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
    try { library_.record({project.directory, project.id}); recents = library_.load(); }
    catch (const std::exception& error) { library_error = QString::fromUtf8(error.what()); }
    QMetaObject::invokeMethod(this, [this, snapshot, failure, library_error, recents = std::move(recents)]() mutable {
      busy_ = false; resume_ = snapshot;
      session_notice_ = failure; library_notice_ = library_error;
      if (recents) recents_ = std::move(*recents);
      update_notice(); emit activated();
    }, Qt::QueuedConnection);
  });
}

void ProjectAccessFlow::inspect(const QString& directory, const QString& expected_id, std::optional<source::ProjectAssets> expected_assets) {
  busy_ = true;
  location_ = QFileInfo(directory).absoluteFilePath();
  error_.clear(); candidates_.clear(); current_available_ = false; current_summary_.clear();
  emit changed();
  launch([this, path = location_, expected_id, expected_assets = std::move(expected_assets)] {
    access_.reset();
    QString failure;
    try {
      access_ = std::make_unique<source::ProjectAccess>(path, expected_id);
      if (expected_assets && source::open_project(path).assets != *expected_assets)
        throw source::SourceError(source::SourceError::Kind::external_change,
            QStringLiteral("Published import at %1 differs from the reviewed bytes. It was retained but not activated; inspect its evidence before opening again.").arg(path));
    }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); access_.reset(); }
    QMetaObject::invokeMethod(this, [this, failure] {
      busy_ = false;
      error_ = failure;
      if (access_) {
        location_ = access_->directory();
        error_ = access_->error();
        current_available_ = access_->current_available();
        if (current_available_) current_summary_ = QStringLiteral("Current project — revision %1\n%2").arg(access_->current_revision()).arg(location_);
        for (const auto& candidate : access_->candidates()) {
          candidates_.push_back(QVariantMap{{"timestamp", candidate.copy.captured_at.toUTC().toString(Qt::ISODateWithMs)},
            {"revision", QVariant::fromValue<qulonglong>(candidate.copy.revision)}, {"reason", candidate.copy.reason},
            {"relation", candidate.relation}, {"location", candidate.copy.directory}, {"origin", candidate.origin_directory}});
        }
      }
      emit changed();
      if (current_available_ && candidates_.empty()) open_current();
    }, Qt::QueuedConnection);
  });
}

void ProjectAccessFlow::open_current() { select(-1, {}); }
void ProjectAccessFlow::recover(int index, const QString& destination) {
  if (index < 0 || index >= candidates_.size()) return;
  select(index, destination);
}

void ProjectAccessFlow::select(int index, const QString& destination) {
  if (busy_ || document_ || review_session_ || intake_ || !access_ || (index < 0 && !current_available_)) return;
  busy_ = true; error_.clear(); emit changed();
  launch([this, index, destination] {
    std::unique_ptr<source::ProjectSession> session;
    QString failure;
    std::optional<source::FountainIntake> intake;
    try { session = index < 0 ? access_->open_current() : access_->recover(static_cast<std::size_t>(index), destination);
      if (session->opened().project.import_review) intake = source::open_fountain_intake(session->opened().assets);
    }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); session.reset(); }
    if (session) access_.reset();
    QMetaObject::invokeMethod(this, [this, session = std::move(session), intake = std::move(intake), failure]() mutable {
      busy_ = false;
      if (!session) { error_ = failure; emit changed(); return; }
      try {
        if (intake) install_intake(std::move(session), std::move(*intake));
        else install(std::move(session));
      }
      catch (const std::exception& error) {
        error_ = QString::fromUtf8(error.what());
        view_.setSource({}); export_job_.reset(); saver_.reset(); document_.reset(); review_session_.reset(); intake_.reset();
        show_access();
        current_available_ = false; candidates_.clear();
        emit changed();
      }
    }, Qt::QueuedConnection);
  });
}

void ProjectAccessFlow::install(std::unique_ptr<source::ProjectSession> session) {
  const auto project = session->opened().project;
  document_ = screenplay_document(session->opened().screenplay.elements, font_);
  saver_ = std::make_unique<Autosaver>(document_.get(), std::move(session));
  export_job_ = std::make_unique<PdfExportJob>(document_.get(), font_directory_, exporter_, project.directory);
  view_.setSource({});
  editor_ = show_editor(view_, *document_, export_job_.get(), project.directory + ".pdf", saver_.get());
  view_.rootObject()->setProperty("projectFlow", QVariant::fromValue<QObject*>(this));
  view_.rootObject()->setProperty("sessionNotice", notice_);
  saver_->guard_close(view_);
  if (resume_ && resume_->project_id == project.id)
    editor_->setProperty("cursorPosition", SessionState::cursor_position(*resume_, *document_));
  editor_->forceActiveFocus();
  connect(editor_, SIGNAL(cursorPositionChanged()), this, SLOT(schedule_session()));
  connect(document_.get(), &QTextDocument::contentsChanged, this, &ProjectAccessFlow::schedule_session);
  connect(saver_.get(), &Autosaver::locationChanged, this, [this] { library_dirty_ = true; schedule_session(); });
  connect(saver_.get(), &Autosaver::leaveReady, this, [this](bool success) {
    if (!success) {
      leaving_ = false; leave_failed_ = true;
      if (state_dirty_) state_timer_.start();
      emit changed(); return;
    }
    saver_drained_ = true;
    finish_leave();
  });
  connect(saver_.get(), &Autosaver::released, this, [this] {
    view_.setSource({});
    export_job_.reset(); saver_.reset(); document_.reset();
    leaving_ = false; saver_drained_ = false; leave_failed_ = false;
    location_.clear(); error_.clear(); candidates_.clear(); current_available_ = false; current_summary_.clear();
    show_access(); emit changed();
  });
  connect(export_job_.get(), &PdfExportJob::completed, this, [this](bool) { finish_leave(); });
  library_dirty_ = true;
  schedule_session();
  emit activated();
}

void ProjectAccessFlow::home() {
  if (busy_ || leaving_) return;
  if (document_) { begin_leave(false); return; }
  busy_ = true; emit changed();
  auto review = std::move(review_session_);
  launch([this, review = std::move(review)]() mutable {
    access_.reset();
    review.reset();
    QMetaObject::invokeMethod(this, [this] {
      intake_.reset();
      location_.clear(); error_.clear(); candidates_.clear(); current_available_ = false; current_summary_.clear();
      busy_ = false; show_access(); emit changed();
    }, Qt::QueuedConnection);
  });
}
void ProjectAccessFlow::retry_leave() { begin_leave(true); }
void ProjectAccessFlow::keep_writing() {
  if (leaving_) return;
  leave_failed_ = false;
  emit changed();
  if (editor_) editor_->forceActiveFocus();
}
void ProjectAccessFlow::begin_leave(bool retry) {
  if (!saver_ || leaving_ || saver_->copy_busy() || saver_->reload_busy()) return;
  state_timer_.stop();
  leaving_ = true; leave_failed_ = false; saver_drained_ = false;
  emit changed();
  saver_->prepare_leave(retry);
}
void ProjectAccessFlow::finish_leave() {
  if (!leaving_ || !saver_drained_ || state_writing_ || export_job_->busy()) return;
  // The saver has drained including recovery I/O, and PDF export is finished.
  // Persist the final cursor on the access worker before releasing this writer.
  state_dirty_ = true;
  persist_session();
}

void ProjectAccessFlow::schedule_session() {
  state_dirty_ = true;
  if (!leaving_ && !state_timer_.isActive()) state_timer_.start();
}
void ProjectAccessFlow::set_notice(const QString& notice) {
  session_notice_ = notice;
  update_notice();
}
void ProjectAccessFlow::update_notice() {
  notice_ = session_notice_;
  if (!library_notice_.isEmpty()) notice_ += (notice_.isEmpty() ? QString() : QStringLiteral("\n")) + library_notice_;
  if (document_ && view_.rootObject()) view_.rootObject()->setProperty("sessionNotice", notice_);
  emit changed();
}

void ProjectAccessFlow::persist_session() {
  if (!saver_ || !document_ || !editor_ || !state_dirty_) return;
  if (state_writing_) { if (!leaving_) state_timer_.start(); return; }
  const auto snapshot = SessionState::capture(saver_->location(), saver_->project_id(), *document_, editor_->property("cursorPosition").toInt());
  const bool record_library = library_dirty_;
  const bool departing = leaving_ && saver_drained_;
  state_dirty_ = false; state_writing_ = true; library_dirty_ = false;
  launch([this, snapshot, record_library, departing] {
    QString failure, library_error;
    std::optional<std::vector<RecentProject>> recents;
    try { state_.save(snapshot); }
    catch (const std::exception& error) { failure = QStringLiteral("Session resume not saved: %1").arg(QString::fromUtf8(error.what())); }
    if (record_library) {
      try { library_.record({snapshot.directory, snapshot.project_id}); recents = library_.load(); }
      catch (const std::exception& error) { library_error = QStringLiteral("Recent projects not saved: %1").arg(QString::fromUtf8(error.what())); }
    }
    QMetaObject::invokeMethod(this, [this, snapshot, failure, record_library, departing, library_error, recents = std::move(recents)]() mutable {
      state_writing_ = false;
      resume_ = snapshot;
      if (record_library) library_notice_ = library_error;
      if (recents) recents_ = std::move(*recents);
      set_notice(failure);
      if (departing && leaving_ && saver_drained_ && !export_job_->busy() && !state_dirty_) {
        // Validate again after export and app-state I/O, then release the
        // project lock on the saver worker. Failure retains the live writer.
        saver_->release_for_leave();
      } else if (leaving_) finish_leave();
      else if (state_dirty_ && !state_timer_.isActive()) state_timer_.start();
    }, Qt::QueuedConnection);
  });
}
}
