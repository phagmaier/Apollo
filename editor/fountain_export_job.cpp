#include "editor/fountain_export_job.h"
#include "editor/screenplay_layout.h"
#include "source/fountain_export.h"
#include "export/snapshot.h"

#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QDateTime>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QEvent>
#include <filesystem>
#include <algorithm>

namespace apollo::editor {
namespace {
QString summary(const QStringList& reasons) {
  return reasons.front() + (reasons.size() > 1 ? QStringLiteral(" (+%1 more; see Export)").arg(reasons.size() - 1) : QString());
}
// Existing paths compare by identity; a new file by its canonical folder.
QString resolved(const QString& path) {
  const QFileInfo info(path);
  if (info.exists()) return info.canonicalFilePath();
  const auto folder = info.absoluteDir().canonicalPath();
  return folder.isEmpty() ? info.absoluteFilePath() : folder + QLatin1Char('/') + info.fileName();
}
}

FountainExportJob::FountainExportJob(QTextDocument* document, QString project, QString omissions, QStringList protected_files, QObject* parent)
    : QObject(parent), document_(document), project_(std::move(project)), omissions_(std::move(omissions)), protected_(std::move(protected_files)) {
  watchdog_.setSingleShot(true);
  watchdog_.setInterval(30000);
  connect(&watchdog_, &QTimer::timeout, this, [this] {
    attention_ = true;
    status_ = QStringLiteral("Fountain snapshot from %1 is taking longer than expected. Cancel Export or Keep Waiting.").arg(captured_);
    emit changed();
  });
}

FountainExportJob::~FountainExportJob() {
  if (process_) {
    process_->disconnect(this);
    process_->kill();
    process_->waitForFinished(1000);
  }
}

QString FountainExportJob::check(bool include_title) const {
  if (!document_) return QStringLiteral("The editor document is closed.");
  return source::fountain_export_blockers(document_elements(*document_), include_title).join('\n');
}

QString FountainExportJob::presentationNote() const {
  return document_ ? source::fountain_presentation_note(document_elements(*document_)) : QString();
}

QString FountainExportJob::refusal(const QString& destination) const {
  if (destination.trimmed().isEmpty()) return QStringLiteral("choose a Fountain destination");
  if (QFileInfo(destination).isDir()) return QStringLiteral("the destination is a folder");
  const auto target = resolved(destination);
  const QFileInfo root(project_);
  const auto project = root.canonicalFilePath().isEmpty() ? root.absoluteFilePath() : root.canonicalFilePath();
  const QFileInfo file(destination);
  const auto parent = file.absoluteDir().canonicalPath();
  const auto inside = [&](const QString& path) { return path == project || path.startsWith(project + '/'); };
  if (inside(target) || inside(file.absoluteFilePath()) || (!parent.isEmpty() && inside(parent + '/' + file.fileName())))
    return QStringLiteral("choose a destination outside the project folder");
  for (const auto& file : protected_)
    if (!file.isEmpty() && target == resolved(file))
      return QStringLiteral("this is the original the project was imported from; Apollo never replaces it");
  return {};
}

bool FountainExportJob::request(const QString& destination, bool include_title) {
  Q_ASSERT(thread() == QThread::currentThread());
  Q_ASSERT(!document_ || document_->thread() == thread());
  if (busy_) return false;
  QString refused = document_ ? refusal(destination) : QStringLiteral("the editor document is closed");
  std::vector<Element> snapshot;
  if (refused.isEmpty()) {
    snapshot = document_elements(*document_);
    const auto reasons = source::fountain_export_blockers(snapshot, include_title);
    if (!reasons.isEmpty()) {
      status_ = QStringLiteral("Fountain export blocked: ") + summary(reasons);
      emit changed();
      return false;
    }
  }
  if (!refused.isEmpty()) {
    status_ = QStringLiteral("Fountain export refused: %1").arg(refused);
    emit changed();
    return false;
  }
  target_ = QFileInfo(destination).exists() ? QFileInfo(destination).canonicalFilePath() : QFileInfo(destination).absoluteFilePath();
  // The helper can only write this owned staging file. Publication belongs to
  // the parent after a clean exit, so cancellation wins even after serialization.
  staged_ = std::make_unique<QTemporaryFile>(QFileInfo(target_).dir().filePath(QStringLiteral(".apollo-fountain-XXXXXX")));
  if (!staged_->open()) {
    status_ = QStringLiteral("Fountain export failed: %1. Any existing file there is unchanged.").arg(staged_->errorString());
    staged_.reset();
    emit changed();
    return false;
  }
  staged_->close();
  const auto payload = encode_export_snapshot({snapshot, include_title});
  busy_ = true;
  cancelling_ = attention_ = false;
  worker_start_ns_ = worker_verified_ns_ = worker_end_ns_ = 0;
  captured_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
  omitted_title_ = !include_title && std::any_of(snapshot.begin(), snapshot.end(),
                                               [](const Element& e) { return source::is_title_field(e.type); });
  note_ = source::fountain_presentation_note(snapshot);
  status_ = QStringLiteral("Exporting Fountain snapshot from %1 to %2 — keep writing").arg(captured_, target_);
  process_ = new QProcess(this);
  connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) finish(false, process_->errorString());
  });
  connect(process_, &QProcess::started, this, [this] { if (cancelling_) process_->kill(); });
  connect(process_, &QProcess::finished, this, [this] { collect(); });
  process_->start(worker_.isEmpty() ? QCoreApplication::applicationDirPath() + "/fountain_export" : worker_, {staged_->fileName()});
  process_->write(payload);
  process_->closeWriteChannel();
  watchdog_.start();
  emit changed();
  return true;
}

void FountainExportJob::guard_close(QWindow& window) {
  if (window_) window_->removeEventFilter(this);
  window_ = &window;
  window.installEventFilter(this);
}

bool FountainExportJob::eventFilter(QObject* watched, QEvent* event) {
  if (watched == window_ && event->type() == QEvent::Close && busy_) {
    event->ignore();
    close_requested_ = true;
    if (!cancelling_) {
      attention_ = true;
      status_ = QStringLiteral("Fountain snapshot from %1 is still exporting. Cancel Export to close, or Keep Waiting.").arg(captured_);
      emit changed();
    }
    return true;
  }
  return QObject::eventFilter(watched, event);
}

void FountainExportJob::cancel() {
  if (!busy_ || cancelling_) return;
  cancelling_ = true;
  attention_ = false;
  watchdog_.stop();
  status_ = QStringLiteral("Cancelling Fountain snapshot from %1…").arg(captured_);
  process_->kill();
  emit changed();
}

void FountainExportJob::keepWaiting() {
  if (!busy_ || cancelling_) return;
  attention_ = false;
  watchdog_.start();
  status_ = QStringLiteral("Exporting Fountain snapshot from %1 to %2 — keep writing").arg(captured_, target_);
  emit changed();
}

void FountainExportJob::collect() {
  QString error = QString::fromUtf8(process_->readAllStandardError()).trimmed();
  const auto interval = process_->readAllStandardOutput().trimmed().split(' ');
  worker_start_ns_ = interval.size() == 3 ? interval[0].toLongLong() : 0;
  worker_verified_ns_ = interval.size() == 3 ? interval[1].toLongLong() : 0;
  worker_end_ns_ = interval.size() == 3 ? interval[2].toLongLong() : 0;
  bool success = !cancelling_ && process_->exitStatus() == QProcess::NormalExit && process_->exitCode() == 0 &&
                 worker_start_ns_ > 0 && worker_verified_ns_ >= worker_start_ns_ && worker_end_ns_ >= worker_verified_ns_;
  if (success) {
    std::error_code rename_error;
    const QFileInfo existing(target_);
    if (existing.isFile() && !QFile::setPermissions(staged_->fileName(), existing.permissions())) {
      success = false;
      error = QStringLiteral("cannot preserve destination permissions");
    }
    if (success) std::filesystem::rename(staged_->fileName().toStdString(), target_.toStdString(), rename_error);
    if (rename_error) { success = false; error = QString::fromStdString(rename_error.message()); }
  }
  if (error.isEmpty()) error = QStringLiteral("worker did not complete cleanly (exit %1)").arg(process_->exitCode());
  finish(success, error);
}

void FountainExportJob::finish(bool success, const QString& error) {
  watchdog_.stop();
  process_->disconnect(this);
  process_->deleteLater();
  process_ = nullptr;
  staged_.reset();
  busy_ = attention_ = false;
  status_ = cancelling_ ? QStringLiteral("Fountain export cancelled. Any existing file there is unchanged.")
          : success ? QStringLiteral("Fountain snapshot from %1 exported to %2. Later edits are not included.").arg(captured_, target_)
                    : QStringLiteral("Fountain export failed: %1. Any existing file there is unchanged.").arg(error);
  if (success) {
    if (omitted_title_) status_ += QStringLiteral(" Title page left out by choice.");
    if (!note_.isEmpty()) status_ += QLatin1Char(' ') + note_;
    if (!omissions_.isEmpty()) status_ += QLatin1Char(' ') + omissions_;
  }
  cancelling_ = false;
  if (close_requested_ && window_) QMetaObject::invokeMethod(window_, &QWindow::close, Qt::QueuedConnection);
  close_requested_ = false;
  emit changed();
  emit completed(success);
}
}
