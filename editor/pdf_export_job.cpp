#include "editor/pdf_export_job.h"
#include "editor/screenplay_layout.h"
#include "export/snapshot.h"

#include <QFileInfo>
#include <QThread>
#include <QDir>
#include <QTemporaryFile>
#include <QDateTime>
#include <QEvent>
#include <filesystem>
#include <stdexcept>

namespace apollo::editor {
PdfExportJob::PdfExportJob(QTextDocument* document, QString fonts, QString worker, QString source, QObject* parent)
    : QObject(parent), document_(document), fonts_(std::move(fonts)), worker_(std::move(worker)), source_(std::move(source)) {
  if (QFileInfo(source_).isDir()) set_project(source_);
  if (document_) unavailable_ = document_->property(export_block_property).toString();
  if (!unavailable_.isEmpty()) status_ = "PDF export unavailable: " + unavailable_;
  watchdog_.setSingleShot(true);
  watchdog_.setInterval(30000);
  connect(&watchdog_, &QTimer::timeout, this, [this] {
    attention_ = true;
    status_ = QStringLiteral("PDF snapshot from %1 is taking longer than expected. Cancel Export or Keep Waiting.").arg(captured_);
    emit changed();
  });
}
PdfExportJob::~PdfExportJob() {
  if (process_) {
    process_->disconnect(this);
    // No event loop is guaranteed during teardown. Never publish a partially
    // completed export or wait indefinitely for a helper that stopped reading.
    process_->kill();
    process_->waitForFinished(1000);
  }
}

void PdfExportJob::set_project(const QString& project) {
  const QFileInfo directory(project);
  project_ = directory.canonicalFilePath();
  if (project_.isEmpty()) project_ = directory.absoluteFilePath();
}

void PdfExportJob::guard_close(QWindow& window) {
  if (window_) window_->removeEventFilter(this);
  window_ = &window;
  window.installEventFilter(this);
}

bool PdfExportJob::eventFilter(QObject* watched, QEvent* event) {
  if (watched == window_ && event->type() == QEvent::Close && busy_) {
    event->ignore();
    close_requested_ = true;
    if (!cancelling_) {
      attention_ = true;
      status_ = QStringLiteral("PDF snapshot from %1 is still exporting. Cancel Export to close, or Keep Waiting.").arg(captured_);
      emit changed();
    }
    return true;
  }
  return QObject::eventFilter(watched, event);
}

void PdfExportJob::cancel() {
  if (!busy_ || cancelling_) return;
  cancelling_ = true;
  attention_ = false;
  watchdog_.stop();
  status_ = QStringLiteral("Cancelling PDF snapshot from %1…").arg(captured_);
  process_->kill();
  emit changed();
}

void PdfExportJob::keepWaiting() {
  if (!busy_ || cancelling_) return;
  attention_ = false;
  watchdog_.start();
  status_ = QStringLiteral("Exporting snapshot from %1 to %2 — keep writing").arg(captured_, destination_);
  emit changed();
}

bool PdfExportJob::request(const QString& destination, bool include_title) {
  Q_ASSERT(thread() == QThread::currentThread());
  Q_ASSERT(!document_ || document_->thread() == thread());
  if (busy_) return false;
  clock_.start();
  worker_start_ns_ = worker_end_ns_ = 0;
  QByteArray payload;
  try {
    if (!document_) throw std::runtime_error("editor document is closed");
    const auto block = document_->property(export_block_property).toString();
    if (!block.isEmpty()) throw std::runtime_error(block.toStdString());
    if (destination.trimmed().isEmpty()) throw std::runtime_error("choose a PDF destination");
    std::error_code error;
    if (!source_.isEmpty() && std::filesystem::equivalent(source_.toStdString(), destination.toStdString(), error))
      throw std::runtime_error("input and PDF destination must differ");
    const QFileInfo target(destination);
    // Resolve aliases of existing files and of the parent for a new file.
    // Also refuse lexical paths inside the project even if a symlink there
    // points outside it. Keep the project root after a missing/moved folder.
    const auto parent = target.absoluteDir().canonicalPath();
    const auto resolved = target.exists() ? target.canonicalFilePath()
                          : parent.isEmpty() ? target.absoluteFilePath() : parent + '/' + target.fileName();
    const auto inside_project = [this](const QString& path) {
      return !project_.isEmpty() && (path == project_ || path.startsWith(project_ + '/'));
    };
    if (inside_project(target.absoluteFilePath()) || inside_project(resolved) ||
        (!parent.isEmpty() && inside_project(parent + '/' + target.fileName())))
      throw std::runtime_error("choose a PDF destination outside the project folder");
    destination_ = target.exists() ? target.canonicalFilePath() : target.absoluteFilePath();
    staged_ = std::make_unique<QTemporaryFile>(QFileInfo(destination_).dir().filePath(QStringLiteral(".apollo-export-XXXXXX")));
    if (!staged_->open()) throw std::runtime_error(staged_->errorString().toStdString());
    staged_->close();
    payload = encode_export_snapshot({document_elements(*document_), include_title});
  } catch (const std::exception& error) {
    staged_.reset();
    status_ = QStringLiteral("PDF export failed: %1").arg(QString::fromUtf8(error.what()));
    emit changed();
    return false;
  }
  busy_ = true;
  cancelling_ = attention_ = false;
  captured_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
  status_ = QStringLiteral("Exporting snapshot from %1 to %2 — keep writing").arg(captured_, destination_);
  process_ = new QProcess(this);
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
  environment.insert(QStringLiteral("QT_IM_MODULE"), QString());
  process_->setProcessEnvironment(environment);
  connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) finish(false, process_->errorString());
  });
  connect(process_, &QProcess::started, this, [this] { if (cancelling_) process_->kill(); });
  connect(process_, &QProcess::finished, this, [this] { collect(); });
  process_->start(worker_, {QStringLiteral("--snapshot-stdin"), staged_->fileName(), fonts_});
  process_->write(payload); // QProcess buffers and flushes without waiting.
  process_->closeWriteChannel();
  capture_ms_ = static_cast<double>(clock_.nsecsElapsed()) / 1e6;
  watchdog_.start();
  emit changed();
  return true;
}

void PdfExportJob::collect() {
  QString error = QString::fromUtf8(process_->readAllStandardError()).trimmed();
  const auto interval = process_->readAllStandardOutput().trimmed().split(' ');
  worker_start_ns_ = interval.size() == 2 ? interval[0].toLongLong() : 0;
  worker_end_ns_ = interval.size() == 2 ? interval[1].toLongLong() : 0;
  bool success = !cancelling_ && process_->exitStatus() == QProcess::NormalExit && process_->exitCode() == 0 &&
                 worker_start_ns_ > 0 && worker_end_ns_ >= worker_start_ns_;
  if (success) {
    // Both files are on the destination filesystem. Only a cleanly exited
    // helper can replace it; even a crash after painting/commit preserves it.
    std::error_code rename_error;
    const QFileInfo existing(destination_);
    if (existing.isFile() && !QFile::setPermissions(staged_->fileName(), existing.permissions())) {
      success = false;
      error = QStringLiteral("cannot preserve destination permissions");
    }
    if (success) std::filesystem::rename(staged_->fileName().toStdString(), destination_.toStdString(), rename_error);
    if (rename_error) { success = false; error = QString::fromStdString(rename_error.message()); }
  }
  if (error.isEmpty()) error = QStringLiteral("worker did not complete cleanly (exit %1)").arg(process_->exitCode());
  finish(success, error);
}

void PdfExportJob::finish(bool success, const QString& error) {
  watchdog_.stop();
  process_->disconnect(this);
  process_->deleteLater();
  process_ = nullptr;
  staged_.reset();
  total_ms_ = static_cast<double>(clock_.nsecsElapsed()) / 1e6;
  busy_ = false;
  attention_ = false;
  status_ = cancelling_ ? QStringLiteral("PDF export cancelled. Any existing file is unchanged.")
          : success ? QStringLiteral("PDF snapshot exported to %1 (captured %2; later edits are not included).").arg(destination_, captured_)
                    : QStringLiteral("PDF export failed: %1. Any existing file is unchanged.").arg(error);
  cancelling_ = false;
  // Queue close before notifying consumers: completion may begin leaving the
  // project. Autosave still handles this new close event and any later edits.
  if (close_requested_ && window_) QMetaObject::invokeMethod(window_, &QWindow::close, Qt::QueuedConnection);
  close_requested_ = false;
  emit changed();
  emit completed(success);
}
}
