#pragma once

#include "source/project.h"

#include <QObject>
#include <QPointer>
#include <QTextDocument>
#include <QTimer>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

class QWindow;

namespace apollo::editor {
// Saves a document to its project's screenplay file while it is edited
// (docs/source-format.md, "Saving"). Every document change is a new
// revision. Within capture_interval_ms of a change, even during continuous
// typing, the GUI thread captures an immutable snapshot; one writer thread
// replaces the file with the newest captured snapshot. Saved means the
// current revision is in the file. A failed save keeps the writing in memory
// and is reported until a later save succeeds.
class Autosaver : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString state READ state NOTIFY statusChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString location READ location CONSTANT)
public:
  static constexpr int capture_interval_ms = 250;
  // capturing/captured/acknowledged run on the GUI thread, the others on the
  // writer, each with the snapshot's revision.
  enum class Phase { capturing, captured, writing, written, committed, acknowledged };
  using Hook = std::function<void(Phase, std::uint64_t revision)>;

  // revision is the one in the project's screenplay file, which the document holds.
  Autosaver(QTextDocument* document, source::Project project, std::uint64_t revision, QObject* parent = nullptr);
  // Captures unsaved changes and waits for the writer to finish.
  ~Autosaver() override;
  // Instrumentation for measurements and fault injection; set before editing.
  void set_hook(Hook hook) { hook_ = std::move(hook); }
  // Saves the current revision now, including after a failure.
  Q_INVOKABLE void retry();
  // Captures now rather than at the end of the interval.
  void flush();
  // Keeps the window open until the writing is saved. While a save is
  // failing, the first close is refused with a warning; a second discards.
  void guard_close(QWindow& window);

  // "saved", "saving", or "failed".
  QString state() const;
  QString status() const;
  QString location() const { return project_.directory; }
  std::uint64_t revision() const { return revision_; }
  std::uint64_t saved_revision() const { return saved_; }

signals:
  void statusChanged();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  struct Snapshot { std::uint64_t revision; std::vector<Element> elements; };
  void changed();
  void capture(bool force);
  void write_loop();
  void finished(std::uint64_t revision, const QString& error);

  QPointer<QTextDocument> document_;
  source::Project project_;
  QTimer timer_;
  Hook hook_;
  std::uint64_t revision_, captured_, saved_;
  QString error_;
  QPointer<QWindow> closing_;
  bool close_requested_ = false, discard_armed_ = false;

  std::mutex mutex_;
  std::condition_variable wake_;
  std::optional<Snapshot> pending_;
  bool stopping_ = false;
  std::thread writer_;
};
}
