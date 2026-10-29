#include "editor/pdf_export_job.h"
#include "editor/screenplay_layout.h"
#include "export/snapshot.h"

#include <QFileInfo>
#include <QThread>
#include <QDir>
#include <QTemporaryFile>
#include <filesystem>
#include <stdexcept>

namespace apollo::editor {
PdfExportJob::PdfExportJob(QTextDocument* document, QString fonts, QString worker, QString source, QObject* parent)
    : QObject(parent), document_(document), fonts_(std::move(fonts)), worker_(std::move(worker)), source_(std::move(source)) {
  if (document_ && !document_->property(export_block_property).toString().isEmpty())
    status_ = "PDF export unavailable: " + document_->property(export_block_property).toString();
}
PdfExportJob::~PdfExportJob() {
  if (process_) {
    process_->disconnect(this);
    // Closing drains the one accepted job, including buffered stdin, before
    // QGuiApplication teardown. No cancellation, detached worker or callback.
    process_->waitForFinished(-1);
    collect(false);
  }
}

bool PdfExportJob::request(const QString& destination, bool include_title) {
  Q_ASSERT(thread() == QThread::currentThread());
  Q_ASSERT(!document_ || document_->thread() == thread());
  if (busy_) return false;
  clock_.start();
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
  status_ = QStringLiteral("Exporting snapshot to %1 — keep writing").arg(destination_);
  process_ = new QProcess(this);
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
  environment.insert(QStringLiteral("QT_IM_MODULE"), QString());
  process_->setProcessEnvironment(environment);
  connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) finish(false, process_->errorString());
  });
  connect(process_, &QProcess::finished, this, [this] { collect(); });
  process_->start(worker_, {QStringLiteral("--snapshot-stdin"), staged_->fileName(), fonts_});
  process_->write(payload); // QProcess buffers and flushes without waiting.
  process_->closeWriteChannel();
  capture_ms_ = static_cast<double>(clock_.nsecsElapsed()) / 1e6;
  emit changed();
  return true;
}

void PdfExportJob::collect(bool notify) {
  QString error = QString::fromUtf8(process_->readAllStandardError()).trimmed();
  const auto interval = process_->readAllStandardOutput().trimmed().split(' ');
  worker_start_ns_ = interval.size() == 2 ? interval[0].toLongLong() : 0;
  worker_end_ns_ = interval.size() == 2 ? interval[1].toLongLong() : 0;
  bool success = process_->exitStatus() == QProcess::NormalExit && process_->exitCode() == 0 &&
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
  if (notify) finish(success, error);
  else staged_.reset();
}

void PdfExportJob::finish(bool success, const QString& error) {
  process_->disconnect(this);
  process_->deleteLater();
  process_ = nullptr;
  staged_.reset();
  total_ms_ = static_cast<double>(clock_.nsecsElapsed()) / 1e6;
  busy_ = false;
  status_ = success ? QStringLiteral("PDF snapshot exported to %1").arg(destination_)
                    : QStringLiteral("PDF export failed: %1").arg(error);
  emit changed();
  emit completed(success);
}
}
