#pragma once

#include "editor/session_state.h"
#include "source/project_access.h"

#include <QFont>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <functional>
#include <memory>
#include <thread>

class QQuickItem;
class QQuickView;
class QTextDocument;

namespace apollo::editor {
class Autosaver;
class PdfExportJob;

// One startup/open operation. No document is installed until an explicit,
// validated choice has acquired edit access. The caller owns the window.
class ProjectAccessFlow : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString location READ location NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString notice READ notice NOTIFY changed)
  Q_PROPERTY(bool currentAvailable READ current_available NOTIFY changed)
  Q_PROPERTY(QString currentSummary READ current_summary NOTIFY changed)
  Q_PROPERTY(QVariantList candidates READ candidates NOTIFY changed)
public:
  ProjectAccessFlow(QQuickView& view, QFont font, QString font_directory, QString exporter, QString session_path = {});
  ~ProjectAccessFlow() override;
  void start(const QString& directory = {});
  Q_INVOKABLE void open_project(const QString& directory);
  Q_INVOKABLE void open_current();
  Q_INVOKABLE void recover(int index, const QString& destination);
  bool busy() const { return busy_; }
  QString location() const { return location_; }
  QString error() const { return error_; }
  QString notice() const { return notice_; }
  bool current_available() const { return current_available_; }
  QString current_summary() const { return current_summary_; }
  QVariantList candidates() const { return candidates_; }
  QTextDocument* document() const { return document_.get(); }
  Autosaver* autosaver() const { return saver_.get(); }
  QQuickItem* editor() const;
signals:
  void changed();
  void activated();
private slots:
  void schedule_session();
private:
  void inspect(const QString& directory, const QString& expected_id);
  void select(int index, const QString& destination);
  void install(std::unique_ptr<source::ProjectSession> session);
  void launch(std::function<void()> operation);
  void persist_session();
  void set_notice(const QString& notice);
  QQuickView& view_;
  QFont font_;
  QString font_directory_, exporter_;
  SessionState state_;
  std::optional<ResumeState> resume_;
  std::unique_ptr<source::ProjectAccess> access_;
  std::unique_ptr<QTextDocument> document_;
  std::unique_ptr<Autosaver> saver_;
  std::unique_ptr<PdfExportJob> export_job_;
  QPointer<QQuickItem> editor_;
  QTimer state_timer_;
  std::thread worker_;
  bool busy_ = false, current_available_ = false, state_writing_ = false, state_dirty_ = false;
  QString location_, error_, notice_, current_summary_;
  QVariantList candidates_;
};
}
