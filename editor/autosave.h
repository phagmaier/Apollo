#pragma once

#include "source/project.h"
#include "source/recovery.h"

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
  Q_PROPERTY(bool reloadBusy READ reload_busy NOTIFY recoveryChanged)
  Q_PROPERTY(QString recoveryStatus READ recovery_status NOTIFY recoveryChanged)
  Q_PROPERTY(QString reloadStatus READ reload_status NOTIFY recoveryChanged)
  Q_PROPERTY(bool closeDialog READ close_dialog NOTIFY recoveryChanged)
  Q_PROPERTY(bool currentProtected READ current_protected NOTIFY recoveryChanged)
public:
  static constexpr int capture_interval_ms = 250;
  // capturing/captured/acknowledged run on the GUI thread, the others on the
  // writer, each with the snapshot's revision.
  enum class Phase { capturing, captured, writing, written, committed, acknowledged, copying, copied, recovering, recovered, reloading, snapshotting, snapshotted };
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
  Q_INVOKABLE bool reload();
  Q_INVOKABLE void keep_open();
  Q_INVOKABLE void close_with_copy_or_discard();
  // Captures now rather than at the end of the interval.
  void flush();
  // Keeps the window open while saving; failures require an explicit close
  // choice after an emergency-copy attempt.
  void guard_close(QWindow& window);

  // "saved", "saving", or "failed".
  QString state() const;
  QString status() const;
  QString location() const { return location_; }
  bool copy_busy() const { return copy_busy_; }
  QString copy_status() const { return copy_status_; }
  QString copy_error() const { return copy_error_; }
  bool reload_busy() const { return reload_busy_; }
  QString recovery_status() const;
  QString reload_status() const { return reload_status_; }
  bool close_dialog() const { return close_dialog_; }
  bool current_protected() const { return protected_revision_ && *protected_revision_ == revision_; }
  std::uint64_t revision() const { return revision_; }
  std::uint64_t saved_revision() const { return saved_; }

signals:
  void statusChanged();
  void locationChanged();
  void copyChanged();
  void copyCompleted(bool success);
  void recoveryChanged();
  void reloadCompleted(bool success);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  struct Snapshot {
    std::uint64_t revision;
    std::vector<Element> elements;
    bool retry;
    bool emergency_only = false;
    QDateTime captured_at = QDateTime::currentDateTimeUtc();
  };
  struct CopyRequest { QString destination; Snapshot snapshot; };
  struct ReloadRequest { Snapshot snapshot; std::optional<source::OpenedProject> candidate; };
  void changed();
  void capture(bool force);
  void write_loop();
  void finished(std::optional<std::uint64_t> revision, const QString& error);
  void copy_finished(std::uint64_t revision, const QString& destination, const QString& error);
  void request_verification();
  void watch_destination();
  void protect_current();
  void recovery_finished(std::uint64_t revision, const std::optional<source::RecoveryCopy>& copy, const QString& error);
  void reload_finished(std::uint64_t revision, const std::optional<source::OpenedProject>& candidate, const QString& error);

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
  bool close_requested_ = false;
  bool copy_busy_ = false;
  QString copy_status_, copy_error_;
  bool reload_busy_ = false, loading_ = false, close_dialog_ = false, allow_close_ = false;
  std::optional<std::uint64_t> protected_revision_;
  QString recovery_path_, recovery_error_, recovery_warning_, reload_status_;
  QDateTime recovery_time_;

  std::mutex mutex_;
  std::condition_variable wake_;
  std::optional<Snapshot> pending_;
  std::optional<CopyRequest> copy_request_;
  std::optional<ReloadRequest> reload_request_;
  // Writer's revision offset after activation, until the GUI acknowledges
  // it. Destruction can queue later writing before that acknowledgment.
  std::optional<std::uint64_t> copy_base_;
  bool verify_requested_ = false;
  std::atomic<bool> paused_{false};
  std::atomic<bool> reload_cancelled_{false};
  bool stopping_ = false;
  std::thread writer_;
};
}
