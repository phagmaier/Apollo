#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTextDocument>
#include <memory>

class QTemporaryFile;

namespace apollo::editor {
// One export at a time. Only request() reads the live document, on its owning
// thread. The helper owns an immutable payload and all its Qt font/paint state.
// Destruction drains an accepted job; closing does not cancel or detach it.
class PdfExportJob : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
public:
  PdfExportJob(QTextDocument* document, QString fonts, QString worker, QString source = {}, QObject* parent = nullptr);
  ~PdfExportJob() override;
  Q_INVOKABLE bool request(const QString& destination, bool include_title = true);
  bool busy() const { return busy_; }
  QString status() const { return status_; }
  double capture_ms() const { return capture_ms_; }
  double total_ms() const { return total_ms_; }
  qint64 worker_pid() const { return process_ ? process_->processId() : 0; }
  qint64 worker_start_ns() const { return worker_start_ns_; }
  qint64 worker_end_ns() const { return worker_end_ns_; }
signals:
  void changed();
  void completed(bool success);
private:
  void collect(bool notify = true);
  void finish(bool success, const QString& error = {});
  QPointer<QTextDocument> document_;
  QString fonts_, worker_, source_, destination_, status_ = QStringLiteral("PDF export ready");
  QProcess* process_ = nullptr;
  std::unique_ptr<QTemporaryFile> staged_;
  QElapsedTimer clock_;
  bool busy_ = false;
  double capture_ms_ = 0, total_ms_ = 0;
  qint64 worker_start_ns_ = 0, worker_end_ns_ = 0;
};
}
