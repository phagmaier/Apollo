#include "editor/fountain_export_job.h"
#include "editor/screenplay_layout.h"
#include "source/fountain_export.h"
#include "source/project.h"

#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QTime>
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
    : QObject(parent), document_(document), project_(std::move(project)), omissions_(std::move(omissions)), protected_(std::move(protected_files)) {}

FountainExportJob::~FountainExportJob() {
  // The queued completion is dropped with this object; the file write is not.
  if (worker_.joinable()) worker_.join();
}

QString FountainExportJob::check(bool include_title) const {
  if (!document_) return QStringLiteral("The editor document is closed.");
  return source::fountain_export_blockers(document_elements(*document_), include_title).join('\n');
}

QString FountainExportJob::caseNote() const {
  return document_ ? source::fountain_case_note(document_elements(*document_)) : QString();
}

QString FountainExportJob::refusal(const QString& destination) const {
  if (destination.trimmed().isEmpty()) return QStringLiteral("choose a Fountain destination");
  if (QFileInfo(destination).isDir()) return QStringLiteral("the destination is a folder");
  const auto target = resolved(destination);
  const auto project = QFileInfo(project_).canonicalFilePath();
  if (!project.isEmpty() && (target == project || target.startsWith(project + QLatin1Char('/'))))
    return QStringLiteral("choose a destination outside the project folder");
  for (const auto& file : protected_)
    if (!file.isEmpty() && target == resolved(file))
      return QStringLiteral("this is the original the project was imported from; Apollo never replaces it");
  return {};
}

bool FountainExportJob::request(const QString& destination, bool include_title) {
  Q_ASSERT(thread() == QThread::currentThread());
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
  if (worker_.joinable()) worker_.join();
  busy_ = true;
  const auto target = QFileInfo(destination).absoluteFilePath();
  const auto captured = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
  const bool omitted_title = !include_title && std::any_of(snapshot.begin(), snapshot.end(),
                                                            [](const Element& e) { return source::is_title_field(e.type); });
  const auto case_note = source::fountain_case_note(snapshot);
  status_ = QStringLiteral("Exporting Fountain snapshot from %1 to %2 — keep writing").arg(captured, target);
  emit changed();
  worker_ = std::thread([this, snapshot = std::move(snapshot), include_title, target, captured, omitted_title, case_note, hook = hook_] {
    bool success = false;
    QString message;
    try {
      const auto result = source::export_fountain(snapshot, include_title);
      if (!result.blockers.isEmpty()) {
        message = QStringLiteral("Fountain export blocked: ") + summary(result.blockers);
      } else {
        if (hook) hook();
        source::replace_file(target, result.bytes);
        success = true;
        message = QStringLiteral("Fountain snapshot from %1 exported to %2.").arg(captured, target);
        if (omitted_title) message += QStringLiteral(" Title page left out by choice.");
        if (!case_note.isEmpty()) message += QLatin1Char(' ') + case_note;
        if (!omissions_.isEmpty()) message += QLatin1Char(' ') + omissions_;
      }
    } catch (const std::exception& error) {
      message = QStringLiteral("Fountain export failed: %1. Any existing file there is unchanged.").arg(QString::fromUtf8(error.what()));
    }
    QMetaObject::invokeMethod(this, [this, success, message] { finish(success, message); }, Qt::QueuedConnection);
  });
  return true;
}

void FountainExportJob::finish(bool success, const QString& status) {
  worker_.join();
  busy_ = false;
  status_ = status;
  emit changed();
  emit completed(success);
}
}
