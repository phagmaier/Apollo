#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTextDocument>
#include <QTimer>
#include <QWindow>
#include <memory>

class QTemporaryFile;

namespace apollo::editor {
// One export at a time. Only request() reads the live document, on its owning
// thread. The helper owns an immutable payload and all its Qt font/paint state.
// Cancellation reaps the helper before removing its staged output. Destruction
// cancels unfinished work; normal window close offers Cancel/Keep Waiting.
class PdfExportJob : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString unavailable READ unavailable CONSTANT)
  Q_PROPERTY(bool attention READ attention NOTIFY changed)
  Q_PROPERTY(bool cancelling READ cancelling NOTIFY changed)
public:
  PdfExportJob(QTextDocument* document, QString fonts, QString worker, QString source = {}, QObject* parent = nullptr);
  ~PdfExportJob() override;
  Q_INVOKABLE bool request(const QString& destination, bool include_title = true);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void keepWaiting();
  void guard_close(QWindow& window);
  void set_project(const QString& project);
  // Timing seam for real-helper tests; production defaults to 30 seconds.
  void set_watchdog_interval(int milliseconds) { watchdog_.setInterval(milliseconds); }
  bool busy() const { return busy_; }
  bool attention() const { return attention_; }
  bool cancelling() const { return cancelling_; }
  QString status() const { return status_; }
  // Why this document can never export PDF, such as imported writing.
  QString unavailable() const { return unavailable_; }
  double capture_ms() const { return capture_ms_; }
  double total_ms() const { return total_ms_; }
  qint64 worker_pid() const { return process_ ? process_->processId() : 0; }
  qint64 worker_start_ns() const { return worker_start_ns_; }
  qint64 worker_end_ns() const { return worker_end_ns_; }
signals:
  void changed();
  void completed(bool success);
private:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void collect();
  void finish(bool success, const QString& error = {});
  QPointer<QTextDocument> document_;
  QString fonts_, worker_, source_, project_, destination_, unavailable_, status_ = QStringLiteral("PDF export ready");
  QProcess* process_ = nullptr;
  std::unique_ptr<QTemporaryFile> staged_;
  QElapsedTimer clock_;
  QTimer watchdog_;
  QPointer<QWindow> window_;
  bool busy_ = false, cancelling_ = false, attention_ = false, close_requested_ = false;
  QString captured_;
  double capture_ms_ = 0, total_ms_ = 0;
  qint64 worker_start_ns_ = 0, worker_end_ns_ = 0;
};
}
