#include "editor/project_access_flow.h"
#include "editor/autosave.h"
#include "editor/editor_view.h"
#include "editor/pdf_export_job.h"
#include "editor/screenplay_layout.h"

#include <QDir>
#include <QFileInfo>
#include <QQuickItem>
#include <QQuickView>
#include <QTextDocument>

namespace apollo::editor {
ProjectAccessFlow::ProjectAccessFlow(QQuickView& view, QFont font, QString font_directory, QString exporter, QString session_path)
    : view_(view), font_(std::move(font)), font_directory_(std::move(font_directory)), exporter_(std::move(exporter)), state_(std::move(session_path)) {
  state_timer_.setSingleShot(true);
  state_timer_.setInterval(250);
  connect(&state_timer_, &QTimer::timeout, this, &ProjectAccessFlow::persist_session);
}
QQuickItem* ProjectAccessFlow::editor() const { return editor_; }

ProjectAccessFlow::~ProjectAccessFlow() {
  state_timer_.stop();
  if (worker_.joinable()) worker_.join();
  // The final cursor is independent of whether writing was saved to its
  // original destination or protected by the close-failure flow.
  if (saver_ && document_ && editor_) {
    try { state_.save(SessionState::capture(saver_->location(), saver_->project_id(), *document_, editor_->property("cursorPosition").toInt())); }
    catch (const std::exception& error) { qWarning("Session resume not saved: %s", error.what()); }
  }
  // Destroy the TextEdit before the document/commands it references.
  view_.setSource({});
  export_job_.reset();
  saver_.reset();
}

void ProjectAccessFlow::launch(std::function<void()> operation) {
  if (worker_.joinable()) worker_.join(); // Previous result has already arrived.
  worker_ = std::thread(std::move(operation));
}

void ProjectAccessFlow::start(const QString& directory) {
  view_.setResizeMode(QQuickView::SizeRootObjectToView);
  view_.setInitialProperties({{"access", QVariant::fromValue<QObject*>(this)}});
  view_.setSource(QUrl::fromLocalFile(QStringLiteral(APOLLO_ACCESS_QML)));
  if (view_.status() != QQuickView::Ready) throw std::runtime_error("cannot load project access QML");
  try { resume_ = state_.load(); }
  catch (const std::exception& error) { set_notice(QStringLiteral("Session resume unavailable: %1").arg(QString::fromUtf8(error.what()))); }
  if (!directory.isEmpty()) inspect(directory, {});
  else if (resume_) inspect(resume_->directory, resume_->project_id);
}

void ProjectAccessFlow::open_project(const QString& directory) {
  if (busy_ || document_) return;
  if (directory.trimmed().isEmpty()) {
    error_ = QStringLiteral("Choose an existing Apollo project folder.");
    emit changed();
    return;
  }
  inspect(directory, {});
}

void ProjectAccessFlow::inspect(const QString& directory, const QString& expected_id) {
  busy_ = true;
  location_ = QFileInfo(directory).absoluteFilePath();
  error_.clear(); candidates_.clear(); current_available_ = false; current_summary_.clear();
  emit changed();
  launch([this, path = location_, expected_id] {
    access_.reset();
    QString failure;
    try { access_ = std::make_unique<source::ProjectAccess>(path, expected_id); }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
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
      // Healthy current saves open directly. Recovery is never automatic,
      // including when it is newer than the authoritative file.
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
  if (busy_ || document_ || !access_ || (index < 0 && !current_available_)) return;
  busy_ = true; error_.clear(); emit changed();
  launch([this, index, destination] {
    std::unique_ptr<source::ProjectSession> session;
    QString failure;
    try { session = index < 0 ? access_->open_current() : access_->recover(static_cast<std::size_t>(index), destination); }
    catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
    QMetaObject::invokeMethod(this, [this, session = std::move(session), failure]() mutable {
      busy_ = false;
      if (!session) { error_ = failure; emit changed(); return; }
      access_.reset();
      try { install(std::move(session)); }
      catch (const std::exception& error) {
        // Do not publish an empty writer as a successful recovery.
        error_ = QString::fromUtf8(error.what());
        view_.setSource({}); export_job_.reset(); saver_.reset(); document_.reset();
        view_.setInitialProperties({{"access", QVariant::fromValue<QObject*>(this)}});
        view_.setSource(QUrl::fromLocalFile(QStringLiteral(APOLLO_ACCESS_QML)));
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
  view_.rootObject()->setProperty("sessionNotice", notice_);
  saver_->guard_close(view_);
  if (resume_ && resume_->project_id == project.id)
    editor_->setProperty("cursorPosition", SessionState::cursor_position(*resume_, *document_));
  editor_->forceActiveFocus();
  connect(editor_, SIGNAL(cursorPositionChanged()), this, SLOT(schedule_session()));
  connect(document_.get(), &QTextDocument::contentsChanged, this, &ProjectAccessFlow::schedule_session);
  connect(saver_.get(), &Autosaver::locationChanged, this, &ProjectAccessFlow::schedule_session);
  schedule_session();
  emit activated();
}

void ProjectAccessFlow::schedule_session() {
  state_dirty_ = true;
  // Bounded capture, not a typing debounce.
  if (!state_timer_.isActive()) state_timer_.start();
}

void ProjectAccessFlow::set_notice(const QString& notice) {
  notice_ = notice;
  if (document_ && view_.rootObject()) view_.rootObject()->setProperty("sessionNotice", notice_);
  emit changed();
}

void ProjectAccessFlow::persist_session() {
  if (!saver_ || !document_ || !editor_ || !state_dirty_) return;
  if (state_writing_) { state_timer_.start(); return; }
  const auto snapshot = SessionState::capture(saver_->location(), saver_->project_id(), *document_, editor_->property("cursorPosition").toInt());
  state_dirty_ = false; state_writing_ = true;
  launch([this, snapshot] {
    QString failure;
    try { state_.save(snapshot); }
    catch (const std::exception& error) { failure = QStringLiteral("Session resume not saved: %1").arg(QString::fromUtf8(error.what())); }
    QMetaObject::invokeMethod(this, [this, failure] {
      state_writing_ = false;
      set_notice(failure);
      if (state_dirty_ && !state_timer_.isActive()) state_timer_.start();
    }, Qt::QueuedConnection);
  });
}
}
