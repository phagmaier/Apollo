#pragma once

#include "engine/layout.h"

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTextDocument>
#include <QProcess>
#include <QTimer>
#include <QWindow>
#include <memory>

class QTemporaryFile;

namespace apollo::editor {
// Clean Fountain of one fixed writing snapshot (docs/fountain-export.md).
// request() reads the live document and runs the explicit eligibility checks
// on its owning thread. A helper verifies/serializes into an owned staging
// file; the parent publishes after a clean exit. Teardown cancels unfinished work.
class FountainExportJob : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString omissions READ omissions CONSTANT)
  Q_PROPERTY(bool attention READ attention NOTIFY changed)
  Q_PROPERTY(bool cancelling READ cancelling NOTIFY changed)
public:
  // An export never replaces the project folder's files or a protected file,
  // such as the external original a project was imported from.
  FountainExportJob(QTextDocument* document, QString project, QString omissions = {}, QStringList protected_files = {},
                    QObject* parent = nullptr);
  ~FountainExportJob() override;
  // The current writing's reasons, one per line; empty when it can be exported.
  Q_INVOKABLE QString check(bool include_title) const;
  // How the current writing's case and spacing are exported
  // (fountain_presentation_note); may be empty.
  Q_INVOKABLE QString presentationNote() const;
  Q_INVOKABLE bool request(const QString& destination, bool include_title = true);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void keepWaiting();
  void guard_close(QWindow& window);
  void set_project(const QString& project) { project_ = project; }
  bool attention() const { return attention_; }
  bool cancelling() const { return cancelling_; }
  // Test seams for actual helper lifecycle checks. Empty restores the helper.
  void set_worker_program(QString program) { worker_ = std::move(program); }
  void set_watchdog_interval(int milliseconds) { watchdog_.setInterval(milliseconds); }
  qint64 worker_pid() const { return process_ ? process_->processId() : 0; }
  qint64 worker_start_ns() const { return worker_start_ns_; }
  qint64 worker_verified_ns() const { return worker_verified_ns_; }
  qint64 worker_end_ns() const { return worker_end_ns_; }
  bool busy() const { return busy_; }
  QString status() const { return status_; }
  QString omissions() const { return omissions_; }
signals:
  void changed();
  void completed(bool success);
private:
  QString refusal(const QString& destination) const;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void collect();
  void finish(bool success, const QString& error);
  QPointer<QTextDocument> document_;
  QString project_, omissions_, status_ = QStringLiteral("Fountain export ready");
  QStringList protected_;
  QString worker_, target_, captured_, note_;
  bool omitted_title_ = false;
  QProcess* process_ = nullptr;
  std::unique_ptr<QTemporaryFile> staged_;
  QTimer watchdog_;
  QPointer<QWindow> window_;
  bool busy_ = false, cancelling_ = false, attention_ = false, close_requested_ = false;
  qint64 worker_start_ns_ = 0, worker_verified_ns_ = 0, worker_end_ns_ = 0;
};
}
