#pragma once

#include "engine/layout.h"

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTextDocument>
#include <functional>
#include <thread>

namespace apollo::editor {
// Clean Fountain of one fixed writing snapshot (docs/fountain-export.md).
// request() reads the live document and runs the explicit eligibility checks
// on its owning thread; verification and the atomic replacement run on a
// worker, so writing continues. Destruction waits for an accepted job.
class FountainExportJob : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString omissions READ omissions CONSTANT)
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
  void set_project(const QString& project) { project_ = project; }
  bool busy() const { return busy_; }
  QString status() const { return status_; }
  QString omissions() const { return omissions_; }
  // Test seam: runs on the worker after verification, before replacement.
  void set_worker_hook(std::function<void()> hook) { hook_ = std::move(hook); }
signals:
  void changed();
  void completed(bool success);
private:
  QString refusal(const QString& destination) const;
  void finish(bool success, const QString& status);
  QPointer<QTextDocument> document_;
  QString project_, omissions_, status_ = QStringLiteral("Fountain export ready");
  QStringList protected_;
  std::function<void()> hook_;
  std::thread worker_;
  bool busy_ = false;
};
}
