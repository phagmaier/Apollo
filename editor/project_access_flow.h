#pragma once

#include "editor/session_state.h"
#include "editor/project_library.h"
#include "source/project_access.h"
#include "source/fountain_intake.h"

#include <QFont>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <memory>
#include <thread>

class QQuickItem;
class QQuickView;
class QTextDocument;

namespace apollo::editor {
class Autosaver;
class PdfExportJob;

// Home, safe access and one active writer. A writer is retained until its
// latest writing and accepted background work have drained successfully.
class ProjectAccessFlow : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString location READ location NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString notice READ notice NOTIFY changed)
  Q_PROPERTY(bool currentAvailable READ current_available NOTIFY changed)
  Q_PROPERTY(QString currentSummary READ current_summary NOTIFY changed)
  Q_PROPERTY(QVariantList candidates READ candidates NOTIFY changed)
  Q_PROPERTY(QVariantList recentProjects READ recent_projects NOTIFY changed)
  Q_PROPERTY(QString defaultLocation READ default_location NOTIFY changed)
  Q_PROPERTY(bool leaving READ leaving NOTIFY changed)
  Q_PROPERTY(bool leaveFailed READ leave_failed NOTIFY changed)
  Q_PROPERTY(bool intakePreview READ intake_preview NOTIFY changed)
  Q_PROPERTY(bool intakeActive READ intake_active NOTIFY changed)
  Q_PROPERTY(QString intakeSummary READ intake_summary NOTIFY changed)
  Q_PROPERTY(QString intakeSource READ intake_source NOTIFY changed)
  Q_PROPERTY(QVariantList intakeFindings READ intake_findings NOTIFY changed)
public:
  ProjectAccessFlow(QQuickView& view, QFont font, QString font_directory, QString exporter, QString session_path = {});
  ~ProjectAccessFlow() override;
  void start(const QString& directory = {}, const QString& fountain = {});
  Q_INVOKABLE void open_project(const QString& directory);
  Q_INVOKABLE void open_current();
  Q_INVOKABLE void recover(int index, const QString& destination);
  Q_INVOKABLE void open_recent(int index);
  Q_INVOKABLE void create_project(const QString& parent, const QString& name);
  Q_INVOKABLE void home();
  Q_INVOKABLE void retry_leave();
  Q_INVOKABLE void keep_writing();
  Q_INVOKABLE void preview_import(const QString& path);
  Q_INVOKABLE void publish_import(const QString& parent, const QString& name);
  Q_INVOKABLE QUrl folder_url(const QString& path) const;
  Q_INVOKABLE QString local_folder(const QUrl& url) const;
  bool busy() const { return busy_; }
  QString location() const { return location_; }
  QString error() const { return error_; }
  QString notice() const { return notice_; }
  bool current_available() const { return current_available_; }
  QString current_summary() const { return current_summary_; }
  QVariantList candidates() const { return candidates_; }
  QVariantList recent_projects() const;
  QString default_location() const { return default_location_; }
  bool leaving() const { return leaving_; }
  bool leave_failed() const { return leave_failed_; }
  bool intake_preview() const { return intake_ && !review_session_; }
  bool intake_active() const { return review_session_ != nullptr; }
  QString intake_summary() const { return intake_ ? intake_->summary : QString(); }
  QString intake_source() const { return intake_ ? intake_->source_view : QString(); }
  QVariantList intake_findings() const { return intake_ ? intake_->findings : QVariantList(); }
  QTextDocument* document() const { return document_.get(); }
  Autosaver* autosaver() const { return saver_.get(); }
  QQuickItem* editor() const;
signals:
  void changed();
  void activated();
private slots:
  void schedule_session();
private:
  void inspect(const QString& directory, const QString& expected_id, std::optional<source::ProjectAssets> expected_assets = {});
  void select(int index, const QString& destination);
  void install(std::unique_ptr<source::ProjectSession> session);
  template <class Operation> void launch(Operation operation) {
    if (worker_.joinable()) worker_.join(); // Previous result has arrived.
    worker_ = std::thread(std::move(operation));
  }
  void persist_session();
  void set_notice(const QString& notice);
  void show_access();
  void begin_leave(bool retry);
  void finish_leave();
  void update_notice();
  void show_intake();
  void install_intake(std::unique_ptr<source::ProjectSession> session, source::FountainIntake intake);
  QQuickView& view_;
  QFont font_;
  QString font_directory_, exporter_;
  SessionState state_;
  ProjectLibrary library_;
  std::vector<RecentProject> recents_;
  std::optional<ResumeState> resume_;
  std::unique_ptr<source::ProjectAccess> access_;
  std::unique_ptr<source::ProjectSession> review_session_;
  std::optional<source::FountainIntake> intake_;
  std::unique_ptr<QTextDocument> document_;
  std::unique_ptr<Autosaver> saver_;
  std::unique_ptr<PdfExportJob> export_job_;
  QPointer<QQuickItem> editor_;
  QTimer state_timer_;
  std::thread worker_;
  bool busy_ = false, current_available_ = false, state_writing_ = false, state_dirty_ = false;
  bool leaving_ = false, leave_failed_ = false, saver_drained_ = false, library_dirty_ = false;
  QString default_location_, session_notice_, library_notice_;
  QString location_, error_, notice_, current_summary_;
  QVariantList candidates_;
};
}
