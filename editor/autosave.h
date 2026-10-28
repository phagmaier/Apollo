#pragma once

#include "source/project.h"

#include <QObject>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QTextDocument>
#include <QTimer>
#include <atomic>
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
  Q_PROPERTY(QString location READ location NOTIFY locationChanged)
  Q_PROPERTY(bool copyBusy READ copy_busy NOTIFY copyChanged)
  Q_PROPERTY(QString copyStatus READ copy_status NOTIFY copyChanged)
  Q_PROPERTY(QString copyError READ copy_error NOTIFY copyChanged)
public:
  static constexpr int capture_interval_ms = 250;
  // capturing/captured/acknowledged run on the GUI thread, the others on the
  // writer, each with the snapshot's revision.
  enum class Phase { capturing, captured, writing, written, committed, acknowledged, copying, copied };
  using Hook = std::function<void(Phase, std::uint64_t revision)>;

  // The document must hold session->opened().screenplay. The session acquired
  // its edit lock before reading and stays locked through the final write.
  Autosaver(QTextDocument* document, std::unique_ptr<source::ProjectSession> session, QObject* parent = nullptr);
  // Captures unsaved changes and waits for the writer to finish.
  ~Autosaver() override;
  // Instrumentation for measurements and fault injection; set before editing.
  void set_hook(Hook hook) { hook_ = std::move(hook); }
  // Saves the current revision now, including after a failure.
  Q_INVOKABLE void retry();
  // Capture the current writing and create a complete new project on the
  // ordered worker. Successful copies become active without resetting the
  // document. Later edits are saved there; failure retains this destination.
  Q_INVOKABLE bool save_copy(const QString& destination);
  // Captures now rather than at the end of the interval.
  void flush();
  // Keeps the window open until the writing is saved. While a save is
  // failing, the first close is refused with a warning; a second discards.
  void guard_close(QWindow& window);

  // "saved", "saving", or "failed".
  QString state() const;
  QString status() const;
  QString location() const { return location_; }
  bool copy_busy() const { return copy_busy_; }
  QString copy_status() const { return copy_status_; }
  QString copy_error() const { return copy_error_; }
  std::uint64_t revision() const { return revision_; }
  std::uint64_t saved_revision() const { return saved_; }

signals:
  void statusChanged();
  void locationChanged();
  void copyChanged();
  void copyCompleted(bool success);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  struct Snapshot { std::uint64_t revision; std::vector<Element> elements; bool retry; };
  struct CopyRequest { QString destination; Snapshot snapshot; };
  void changed();
  void capture(bool force);
  void write_loop();
  void finished(std::optional<std::uint64_t> revision, const QString& error);
  void copy_finished(std::uint64_t revision, const QString& destination, const QString& error);
  void request_verification();
  void watch_destination();

  QPointer<QTextDocument> document_;
  std::unique_ptr<source::ProjectSession> session_;
  // GUI-thread location; session_ is replaced only on the writer.
  QString location_, watched_source_;
  QTimer timer_;
  QTimer verification_timer_;
  QFileSystemWatcher watcher_;
  Hook hook_;
  std::uint64_t revision_, captured_, saved_;
  QString error_;
  QPointer<QWindow> closing_;
  bool close_requested_ = false, discard_armed_ = false;
  bool copy_busy_ = false;
  QString copy_status_, copy_error_;

  std::mutex mutex_;
  std::condition_variable wake_;
  std::optional<Snapshot> pending_;
  std::optional<CopyRequest> copy_request_;
  // Writer's revision offset after activation, until the GUI acknowledges
  // it. Destruction can queue later writing before that acknowledgment.
  std::optional<std::uint64_t> copy_base_;
  bool verify_requested_ = false;
  std::atomic<bool> paused_{false};
  bool stopping_ = false;
  std::thread writer_;
};
}
