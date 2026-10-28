#include "editor/project_access_flow.h"
#include "editor/autosave.h"
#include "editor/pdf_export_job.h"
#include "editor/screenplay_layout.h"
#include "source/recovery.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickView>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QSignalSpy>
#include <csignal>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <QTextCursor>
#include <QThread>
#include <QtTest>

using namespace apollo;
using namespace apollo::editor;
using namespace apollo::source;

namespace {
QByteArray bytes(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return file.readAll();
}
QQuickItem* find_item(QQuickItem* root, const QString& name) {
  if (root->objectName() == name) return root;
  for (auto* child : root->childItems())
    if (auto* found = find_item(child, name)) return found;
  return nullptr;
}
struct Window {
  QQuickView view;
  ProjectAccessFlow flow;
  Window(const QFont& font, const QString& state, const QString& project = {}, const QString& exporter = {}, const QString& fountain = {})
      : flow(view, font, QStringLiteral(APOLLO_FONT_DIR),
             exporter.isEmpty() ? QCoreApplication::applicationDirPath() + "/pdf_export" : exporter, state) {
    view.resize(1000, 1000);
    flow.start(project, fountain);
    view.show();
  }
  QQuickItem* item(const char* name) {
    if (auto* found = view.rootObject()->findChild<QQuickItem*>(QString::fromLatin1(name))) return found;
    return find_item(view.rootObject(), QString::fromLatin1(name));
  }
  void type(const QString& text) {
    for (const auto ch : text) {
      QKeyEvent press(QEvent::KeyPress, ch.toUpper().unicode(), Qt::NoModifier, QString(ch));
      QKeyEvent release(QEvent::KeyRelease, ch.toUpper().unicode(), Qt::NoModifier, QString(ch));
      QCoreApplication::sendEvent(&view, &press);
      QCoreApplication::sendEvent(&view, &release);
    }
  }
  bool click(const char* name) {
    QTest::qWait(30); // Let one frame polish changed form/Popup geometry.
    auto* button = item(name);
    if (!button || !button->isVisible() || !button->isEnabled()) return false;
    // ScrollView controls are real mouse targets, including those below the fold.
    auto* content = button->parentItem();
    while (content && !content->inherits("QQuickFlickable")) content = content->parentItem();
    if (content) {
      const auto y = button->mapToItem(content, QPointF()).y() + content->property("contentY").toReal();
      const auto maximum = qMax(0.0, content->property("contentHeight").toReal() - content->height());
      content->setProperty("contentY", qBound(0.0, y - 40, maximum));
    }
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, button->mapToScene(button->boundingRect().center()).toPoint());
    return true;
  }
};
}

class ProjectAccessFlowTests : public QObject {
  Q_OBJECT
  QTemporaryDir root_;
  QFont font_;
  int count_ = 0;
  QString path(const QString& label) { return root_.filePath(label + QString::number(++count_)); }
  Project project() { return create_project(path("project"), {5, {{ElementType::action, "Current writing.", false, {}, "first"},
                                                               {ElementType::action, "Second paragraph.", false, {}, "second"}}}); }
private slots:
  void initTestCase() {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    qputenv("XDG_DATA_HOME", root_.filePath("data").toUtf8());
    font_ = courier_prime(QStringLiteral(APOLLO_FONT_DIR));
    QVERIFY(QDir().mkpath(QStringLiteral(APOLLO_ARTIFACT_DIR)));
  }
  void latest_current_opens_and_resumes_cursor_without_rewriting_source() {
    const auto p = project();
    const auto original = bytes(p.screenplay_path());
    const auto state = path("session");
    {
      Window window(font_, state, p.directory);
      QTRY_VERIFY(window.flow.document());
      QCOMPARE(window.flow.document()->toPlainText(), QStringLiteral("Current writing.\nSecond paragraph."));
      window.flow.editor()->setProperty("cursorPosition", 20);
      QTRY_VERIFY(SessionState(state).load().has_value());
      QTRY_COMPARE(SessionState(state).load()->position, 20);
    }
    QCOMPARE(bytes(p.screenplay_path()), original);
    {
      Window resumed(font_, state);
      QTRY_VERIFY(resumed.flow.document());
      QCOMPARE(resumed.flow.autosaver()->location(), p.directory);
      QCOMPARE(resumed.flow.editor()->property("cursorPosition").toInt(), 20);
      QTest::keyClick(&resumed.view, Qt::Key_X);
      QTRY_COMPARE(resumed.flow.autosaver()->state(), QStringLiteral("saved"));
      QCOMPARE(open_project(p.directory).screenplay.elements.at(1).text, std::string("Secxond paragraph."));
    }
  }
  void damaged_save_requires_explicit_timestamped_recovery_and_preserves_evidence() {
    const auto p = project();
    const auto original = open_project(p.directory);
    RecoveryStore store(p, {}, p.directory + "/.recovery");
    auto latest = original.screenplay;
    latest.revision = 7;
    latest.elements.front().text = "Recovered latest writing.";
    const auto copy = store.preserve(latest, "emergency", QDateTime::fromString("2026-10-28T12:30:00.000Z", Qt::ISODateWithMs));
    const auto evidence = bytes(copy.directory + "/screenplay.json");
    replace_file(p.screenplay_path(), "{damaged latest save\n");
    const auto damaged = bytes(p.screenplay_path());
    const auto state = path("session");
    Window window(font_, state, p.directory);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(!window.flow.current_available());
    QCOMPARE(window.flow.candidates().size(), 1);
    const auto row = window.flow.candidates().front().toMap();
    QCOMPARE(row.value("revision").toULongLong(), 7u);
    QCOMPARE(QDateTime::fromString(row.value("timestamp").toString(), Qt::ISODateWithMs),
             QDateTime::fromString("2026-10-28T12:30:00.000Z", Qt::ISODateWithMs));
    const auto destination = path("recovered");
    window.item("recoveryDestination")->setProperty("text", destination);
    QVERIFY(!window.item("recoverProject")->isEnabled());
    QVERIFY(!window.flow.document());
    QVERIFY(!SessionState(state).load());
    QTest::qWait(50);
    QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/damaged-save.png")));
    QVERIFY(window.click("recoveryChoice0"));
    QVERIFY(window.click("recoverProject"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.document()->toPlainText(), QStringLiteral("Recovered latest writing.\nSecond paragraph."));
    QCOMPARE(window.flow.autosaver()->location(), destination);
    QVERIFY(window.flow.autosaver()->project_id() != p.id);
    QCOMPARE(bytes(p.screenplay_path()), damaged);
    QCOMPARE(bytes(copy.directory + "/screenplay.json"), evidence);
    QTest::keyClick(&window.view, Qt::Key_Z);
    QTRY_COMPARE(window.flow.autosaver()->state(), QStringLiteral("saved"));
    QCOMPARE(open_project(destination).screenplay.elements.front().text, std::string("zRecovered latest writing."));
    QCOMPARE(bytes(p.screenplay_path()), damaged);
    QTRY_VERIFY(SessionState(state).load().has_value());
    QTRY_COMPARE(SessionState(state).load()->directory, destination);
  }
  void newer_emergency_requires_choice_and_never_rolls_back_current() {
    const auto p = project();
    const auto before = bytes(p.screenplay_path());
    auto newer = open_project(p.directory).screenplay;
    newer.revision = 9; newer.elements.front().text = "Emergency writing.";
    RecoveryStore emergency(p, {}, RecoveryStore::emergency_directory(p));
    const auto copy = emergency.preserve(newer, "emergency", QDateTime::currentDateTimeUtc().addDays(-1));
    Window window(font_, path("session"), p.directory);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(window.flow.current_available());
    QCOMPARE(window.flow.candidates().size(), 1);
    QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/reconcile.png")));
    QVERIFY(window.click("openCurrent"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.document()->toPlainText(), QStringLiteral("Current writing.\nSecond paragraph."));
    QCOMPARE(window.flow.autosaver()->location(), p.directory);
    QCOMPARE(bytes(p.screenplay_path()), before);
    QCOMPARE(open_project(copy.directory).screenplay.elements.front().text, std::string("Emergency writing."));
  }
  void stale_selection_and_collisions_do_not_activate_or_destroy_evidence() {
    const auto p = project();
    RecoveryStore store(p, {}, p.directory + "/.recovery");
    const auto copy = store.preserve(open_project(p.directory).screenplay, "autosave");
    replace_file(p.screenplay_path(), "broken");
    Window window(font_, path("session"), p.directory);
    QTRY_VERIFY(!window.flow.busy());
    const auto occupied = path("occupied");
    QVERIFY(QDir().mkdir(occupied));
    QVERIFY(window.click("recoveryChoice0"));
    window.item("recoveryDestination")->setProperty("text", occupied);
    QVERIFY(window.click("recoverProject"));
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(window.flow.error().contains("exists"));
    QCOMPARE(bytes(p.screenplay_path()), QByteArray("broken"));
    replace_file(copy.directory + "/screenplay.json", "changed evidence");
    QVERIFY(window.click("recoveryChoice0"));
    const auto destination = path("stale");
    window.item("recoveryDestination")->setProperty("text", destination);
    QVERIFY(window.click("recoverProject"));
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(!window.flow.error().isEmpty());
    QVERIFY(!QFileInfo::exists(destination));
    QCOMPARE(bytes(copy.directory + "/screenplay.json"), QByteArray("changed evidence"));
  }
  void unavailable_resume_keeps_last_state_and_allows_explicit_other_open() {
    const auto missing = path("missing");
    const auto p = project();
    const auto state = path("session");
    SessionState(state).save({missing, p.id, "first", 3, 3});
    const auto before = bytes(state);
    Window window(font_, state);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(window.flow.error().contains(missing));
    QCOMPARE(bytes(state), before);
    window.item("openLocation")->setProperty("text", p.directory);
    QVERIFY(window.click("openProject"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.editor()->property("cursorPosition").toInt(), 3);
  }
  void save_copy_updates_resume_identity_and_malformed_state_is_visible() {
    const auto p = project();
    const auto state = path("session");
    const auto destination = path("copy");
    {
      Window window(font_, state, p.directory);
      QTRY_VERIFY(window.flow.document());
      window.flow.editor()->setProperty("cursorPosition", 4);
      QVERIFY(window.flow.autosaver()->save_copy(destination));
      QTRY_COMPARE(window.flow.autosaver()->location(), destination);
      QTRY_VERIFY(SessionState(state).load().has_value());
      QTRY_COMPARE(SessionState(state).load()->directory, destination);
      QCOMPARE(SessionState(state).load()->project_id, open_project(destination).project.id);
    }
    {
      Window resumed(font_, state);
      QTRY_VERIFY(resumed.flow.document());
      QCOMPARE(resumed.flow.autosaver()->location(), destination);
      QCOMPARE(resumed.flow.editor()->property("cursorPosition").toInt(), 4);
    }
    replace_file(state, "broken session evidence");
    {
      Window explicit_open(font_, state, p.directory);
      QTRY_VERIFY(explicit_open.flow.document());
      QTRY_VERIFY(explicit_open.flow.notice().contains("Session resume"));
      QCOMPARE(explicit_open.item("sessionNotice")->property("text").toString(), explicit_open.flow.notice());
      QCOMPARE(bytes(state), QByteArray("broken session evidence"));
    }
    QCOMPARE(bytes(state), QByteArray("broken session evidence"));
  }
  void failed_copy_keeps_resume_identity_and_final_cursor_is_captured_on_close() {
    const auto p = project();
    const auto occupied = project();
    const auto occupied_bytes = bytes(occupied.screenplay_path());
    const auto state = path("session");
    {
      Window window(font_, state, p.directory);
      QTRY_VERIFY(window.flow.document());
      QVERIFY(window.flow.autosaver()->save_copy(occupied.directory));
      QTRY_VERIFY(!window.flow.autosaver()->copy_busy());
      QVERIFY(!window.flow.autosaver()->copy_error().isEmpty());
      QTRY_VERIFY(SessionState(state).load().has_value());
      QCOMPARE(SessionState(state).load()->directory, p.directory);
      QCOMPARE(SessionState(state).load()->project_id, p.id);
      QCOMPARE(bytes(occupied.screenplay_path()), occupied_bytes);
      window.flow.editor()->setProperty("cursorPosition", 8);
      // No timer/event-loop turn before teardown: final capture must win.
    }
    QCOMPARE(SessionState(state).load()->position, 8);
    Window resumed(font_, state);
    QTRY_VERIFY(resumed.flow.document());
    QCOMPARE(resumed.flow.autosaver()->location(), p.directory);
    QCOMPARE(resumed.flow.editor()->property("cursorPosition").toInt(), 8);
  }
  void interrupted_inspection_preserves_resume_evidence_and_releases_lock() {
    const auto p = project();
    const auto remembered = project();
    const auto state = path("session");
    SessionState(state).save({remembered.directory, remembered.id, "first", 2, 2});
    const auto before_state = bytes(state);
    const auto before_source = bytes(p.screenplay_path());
    {
      Window interrupted(font_, state, p.directory);
      // Destroy while the worker result is still queued, before activation.
    }
    QCOMPARE(bytes(state), before_state);
    QCOMPARE(bytes(p.screenplay_path()), before_source);
    ProjectSession unlocked(p.directory);
    QCOMPARE(unlocked.opened().project.id, p.id);
  }
  void replaced_resume_location_does_not_implicitly_open_a_different_identity() {
    const auto remembered = project();
    const auto replacement = project();
    const auto state = path("session");
    SessionState(state).save({replacement.directory, remembered.id, "first", 2, 2});
    const auto before_state = bytes(state);
    const auto before_source = bytes(replacement.screenplay_path());
    Window window(font_, state);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(!window.flow.current_available());
    QCOMPARE(bytes(state), before_state);
    QCOMPARE(bytes(replacement.screenplay_path()), before_source);
    // Explicit opening, rather than resume, can choose that replacement.
    window.item("openLocation")->setProperty("text", replacement.directory);
    QVERIFY(window.click("openProject"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.autosaver()->project_id(), replacement.id);
  }
  void home_flushes_latest_writing_and_switches_without_cross_project_edits() {
    const auto first = project(), second = project();
    const auto second_before = bytes(second.screenplay_path());
    const auto state = path("session");
    Window window(font_, state, first.directory);
    QTRY_VERIFY(window.flow.document());
    window.flow.editor()->setProperty("cursorPosition", 0);
    window.flow.editor()->forceActiveFocus();
    window.type(QStringLiteral("Latest "));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(!window.flow.document());
    QTRY_VERIFY(!window.flow.busy());
    QCOMPARE(open_project(first.directory).screenplay.elements.front().text, std::string("Latest Current writing."));
    QCOMPARE(bytes(second.screenplay_path()), second_before);
    { ProjectSession released(first.directory); }
    QVERIFY(window.item("recentProject0"));
    window.item("openLocation")->setProperty("text", second.directory);
    QVERIFY(window.click("openProject"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.autosaver()->project_id(), second.id);
    QCOMPARE(window.flow.document()->toPlainText(), QStringLiteral("Current writing.\nSecond paragraph."));
    window.type(QStringLiteral("Other "));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(!window.flow.document());
    QTRY_VERIFY(!window.flow.busy());
    QCOMPARE(open_project(second.directory).screenplay.elements.front().text, std::string("Other Current writing."));
    QCOMPARE(open_project(first.directory).screenplay.elements.front().text, std::string("Latest Current writing."));
    auto* recent = window.item("recentProject1");
    QVERIFY(recent);
    window.view.rootObject()->forceActiveFocus();
    for (int key = 0; key < 12 && !recent->hasActiveFocus(); ++key) QTest::keyClick(&window.view, Qt::Key_Tab);
    QVERIFY(recent->hasActiveFocus());
    QTest::keyClick(&window.view, Qt::Key_Space);
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.autosaver()->project_id(), first.id);
    QCOMPARE(window.flow.document()->toPlainText(), QStringLiteral("Latest Current writing.\nSecond paragraph."));
  }

  void failed_leave_keeps_writing_and_undo_until_retry_succeeds() {
    const auto p = project();
    const auto state = path("session");
    Window window(font_, state, p.directory);
    QTRY_VERIFY(window.flow.document());
    const auto moved = path("moved");
    QVERIFY(QDir().rename(p.directory, moved));
    window.flow.editor()->forceActiveFocus();
    window.type(QStringLiteral("Retained "));
    auto* document = window.flow.document();
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(window.item("leaveKeepWriting") && window.item("leaveKeepWriting")->isVisible());
    QCOMPARE(window.flow.document(), document);
    QCOMPARE(document->toPlainText(), QStringLiteral("Retained Current writing.\nSecond paragraph."));
    QVERIFY(document->isUndoAvailable());
    QVERIFY(window.click("leaveKeepWriting"));
    QCOMPARE(window.flow.document(), document);
    QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(document->toPlainText(), QStringLiteral("Current writing.\nSecond paragraph."));
    QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(document->toPlainText(), QStringLiteral("Retained Current writing.\nSecond paragraph."));
    QVERIFY(QDir().rename(moved, p.directory));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(window.item("leaveRetry") && window.item("leaveRetry")->isVisible());
    QVERIFY(window.click("leaveRetry"));
    QTRY_VERIFY(!window.flow.document());
    QTRY_VERIFY(!window.flow.busy());
    QCOMPARE(open_project(p.directory).screenplay.elements.front().text, std::string("Retained Current writing."));
  }
  void new_project_controls_handle_collisions_locations_and_failure_without_empty_activation() {
    const auto state = path("session");
    const auto parent = path("chosen-parent");
    QVERIFY(QDir().mkpath(parent));
    replace_file(parent + "/My script", "Retained collision.");
    Window window(font_, state);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/home.png")));
    QVERIFY(window.click("showNewProject"));
    window.item("newProjectName")->setProperty("text", "My script");
    window.item("newProjectParent")->setProperty("text", parent + "/missing");
    QVERIFY(window.click("createProject"));
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.error().isEmpty());
    QVERIFY(!window.flow.document());
    QVERIFY(!SessionState(state).load());
    QVERIFY(ProjectLibrary(state + ".recents").load().empty());
    QVERIFY(!QFileInfo::exists(parent + "/missing"));
    window.item("newProjectParent")->setProperty("text", parent);
    QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/new-project.png")));
    QVERIFY(window.click("createProject"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.autosaver()->location(), parent + "/My script (2)");
    QCOMPARE(window.flow.document()->toPlainText(), QString());
    window.type(QStringLiteral("A new opening."));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(!window.flow.document());
    QCOMPARE(bytes(parent + "/My script"), QByteArray("Retained collision."));
    QCOMPARE(open_project(parent + "/My script (2)").screenplay.elements.front().text, std::string("A new opening."));
    QCOMPARE(ProjectLibrary(state + ".recents").load().front().directory, parent + "/My script (2)");
    QVERIFY(window.click("recentProject0"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.document()->toPlainText(), QStringLiteral("A new opening."));
  }

  void stale_recent_identity_and_moved_project_use_safe_access_without_replacing_last_success() {
    const auto p = project(), replacement = project();
    const auto state = path("session");
    ProjectLibrary(state + ".recents").record({p.directory, p.id});
    const auto moved = path("moved");
    QVERIFY(QDir().rename(p.directory, moved));
    QVERIFY(QDir().rename(replacement.directory, p.directory));
    const auto replacement_before = bytes(p.screenplay_path());
    Window window(font_, state);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(window.click("recentProject0"));
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document());
    QVERIFY(!window.flow.current_available());
    QVERIFY(!window.flow.error().isEmpty());
    QCOMPARE(bytes(p.screenplay_path()), replacement_before);
    QVERIFY(!SessionState(state).load());
    QCOMPARE(ProjectLibrary(state + ".recents").load().front().project_id, p.id);
    window.item("openLocation")->setProperty("text", moved);
    QVERIFY(window.click("openProject"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.autosaver()->project_id(), p.id);
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(!window.flow.document());
    QCOMPARE(ProjectLibrary(state + ".recents").load().size(), std::size_t(1));
    QCOMPARE(ProjectLibrary(state + ".recents").load().front().directory, moved);
    QVERIFY(window.click("recentProject0"));
    QTRY_VERIFY(window.flow.document());
    QCOMPARE(window.flow.autosaver()->location(), moved);
  }

  void both_save_destinations_failing_keep_memory_and_copy_can_enable_safe_departure() {
    const auto p = project();
    const auto state = path("session");
    Window window(font_, state, p.directory);
    QTRY_VERIFY(window.flow.document());
    const auto emergency = RecoveryStore::emergency_directory(p);
    QVERIFY(QDir().mkpath(QFileInfo(emergency).absolutePath()));
    replace_file(emergency, "Unavailable emergency destination.");
    const auto moved = path("moved");
    QVERIFY(QDir().rename(p.directory, moved));
    const auto original_before = bytes(moved + "/screenplay.json");
    window.type(QStringLiteral("Keep this "));
    auto* document = window.flow.document();
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(window.flow.leave_failed());
    QVERIFY(!window.flow.autosaver()->current_protected());
    QCOMPARE(window.flow.document(), document);
    QCOMPARE(document->toPlainText(), QStringLiteral("Keep this Current writing.\nSecond paragraph."));
    QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/leave-failure.png")));
    QVERIFY(window.click("leaveSaveCopy"));
    window.item("copyDestination")->setProperty("text", moved);
    QVERIFY(window.click("copyConfirm"));
    QTRY_VERIFY(!window.flow.autosaver()->copy_busy());
    QVERIFY(!window.flow.autosaver()->copy_error().isEmpty());
    QCOMPARE(window.flow.document(), document);
    QCOMPARE(document->toPlainText(), QStringLiteral("Keep this Current writing.\nSecond paragraph."));
    QCOMPARE(bytes(moved + "/screenplay.json"), original_before);
    const auto copy = path("safe-copy");
    window.item("copyDestination")->setProperty("text", copy);
    QVERIFY(window.click("copyConfirm"));
    QTRY_COMPARE(window.flow.autosaver()->location(), copy);
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(!window.flow.document());
    QCOMPARE(open_project(copy).screenplay.elements.front().text, std::string("Keep this Current writing."));
    QCOMPARE(bytes(moved + "/screenplay.json"), original_before);
    QCOMPARE(SessionState(state).load()->directory, copy);
  }

  void leaving_waits_for_worker_recovery_and_pdf_without_blocking_the_gui() {
    const auto p = project();
    std::mutex mutex;
    std::condition_variable condition;
    bool released = false;
    std::atomic<bool> entered = false;
    Window window(font_, path("session"), p.directory);
    QTRY_VERIFY(window.flow.document());
    auto* saver = window.flow.autosaver();
    saver->set_hook([&](Autosaver::Phase phase, std::uint64_t) {
      if (phase != Autosaver::Phase::snapshotting) return;
      std::unique_lock lock(mutex);
      entered = true;
      condition.wait(lock, [&] { return released; });
    });
    const auto release = qScopeGuard([&] {
      { std::lock_guard lock(mutex); released = true; }
      condition.notify_all();
    });
    auto* job = qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>());
    QVERIFY(job);
    const auto pdf = path("export") + ".pdf";
    QVERIFY(job->request(pdf));
    QVERIFY(job->busy());
    window.type(QStringLiteral("Drained "));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(entered.load());
    QVERIFY(window.flow.leaving());
    QVERIFY(window.flow.document());
    bool heartbeat = false;
    QTimer::singleShot(0, [&] { heartbeat = true; });
    QTRY_VERIFY(heartbeat);
    QVERIFY(window.flow.document());
    {
      std::lock_guard lock(mutex);
      released = true;
    }
    condition.notify_all();
    QTRY_VERIFY(!window.flow.document());
    QCOMPARE(open_project(p.directory).screenplay.elements.front().text, std::string("Drained Current writing."));
    QVERIFY(bytes(pdf).startsWith("%PDF-"));
    { ProjectSession released_session(p.directory); }
  }

  void broken_recent_list_is_visible_and_does_not_block_writing_or_get_overwritten() {
    const auto p = project();
    const auto state = path("session");
    replace_file(state + ".recents", "Retained malformed recent list.");
    Window window(font_, state, p.directory);
    QTRY_VERIFY(window.flow.document());
    QTRY_VERIFY(window.flow.notice().contains("Recent projects"));
    window.type(QStringLiteral("Saved independently "));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(!window.flow.document());
    QCOMPARE(bytes(state + ".recents"), QByteArray("Retained malformed recent list."));
    QCOMPARE(open_project(p.directory).screenplay.elements.front().text, std::string("Saved independently Current writing."));
    QVERIFY(window.item("accessNotice")->property("text").toString().contains("Recent projects"));
  }
  void destination_failure_while_export_finishes_keeps_the_live_writer() {
    const auto p = project();
    const auto wrapper = path("held-exporter");
    replace_file(wrapper, "#!/bin/sh\nkill -STOP $$\nexec \"" +
        (QCoreApplication::applicationDirPath() + "/pdf_export").toUtf8() + "\" \"$@\"\n");
    QVERIFY(QFile::setPermissions(wrapper, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    Window window(font_, path("session"), p.directory, wrapper);
    QTRY_VERIFY(window.flow.document());
    auto* document = window.flow.document();
    auto* job = qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>());
    QVERIFY(job && job->request(path("held") + ".pdf"));
    QTRY_VERIFY(job->worker_pid() > 0);
    const auto pid = static_cast<pid_t>(job->worker_pid());
    const auto resume_worker = qScopeGuard([&] { ::kill(pid, SIGCONT); });
    QSignalSpy ready(window.flow.autosaver(), &Autosaver::leaveReady);
    window.type(QStringLiteral("Saved before export "));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(!ready.empty());
    QVERIFY(ready.front().front().toBool());
    QVERIFY(window.flow.document());
    const auto moved = path("moved-after-save");
    QVERIFY(QDir().rename(p.directory, moved));
    QCOMPARE(::kill(pid, SIGCONT), 0);
    QTRY_VERIFY(window.flow.leave_failed());
    QCOMPARE(window.flow.document(), document);
    QCOMPARE(document->toPlainText(), QStringLiteral("Saved before export Current writing.\nSecond paragraph."));
    QCOMPARE(open_project(moved).screenplay.elements.front().text, std::string("Saved before export Current writing."));
    QVERIFY(window.click("leaveKeepWriting"));
  }

  void import_review_publish_reopen_and_switch_through_real_controls() {
    const auto state = path("intake-session");
    const auto source = path("input") + ".fountain";
    const QByteArray original("\xef\xbb\xbf" "Title: Retained\r\n\r\nA line. [[<b>note</b>]]\r\n/*omitted*/\r\n# Section\r\n");
    replace_file(source, original);
    const auto parent = path("intake-parent");
    QVERIFY(QDir().mkpath(parent));
    replace_file(parent + "/Imported", "collision evidence");
    const auto writing = project();
    QString published;
    QByteArray metadata;
    {
      Window window(font_, state);
      QTRY_VERIFY(!window.flow.busy());
      QVERIFY(window.click("showImport"));
      window.item("importSourcePath")->setProperty("text", source);
      QVERIFY(window.click("previewImport"));
      QTRY_VERIFY(window.flow.intake_preview());
      QVERIFY(window.flow.intake_summary().contains("entire source is pending"));
      QVERIFY(!window.flow.document());
      QVERIFY(!window.flow.autosaver());
      QCOMPARE(bytes(source), original);
      QVERIFY(!SessionState(state).load());
      QVERIFY(ProjectLibrary(state + ".recents").load().empty());
      auto* reader = window.item("intakeSource");
      QVERIFY(reader && reader->property("readOnly").toBool());
      const auto shown = reader->property("text").toString();
      QVERIFY(shown.contains("[[<b>note</b>]]"));
      QVERIFY(window.item("intakeFindingText")->property("text").toString().contains("[[<b>note</b>]]"));
      reader->forceActiveFocus();
      window.type("Must not edit");
      QCOMPARE(reader->property("text").toString(), shown);
      window.item("intakeProjectName")->setProperty("text", "Imported");
      window.item("intakeProjectParent")->setProperty("text", parent + "/missing");
      QVERIFY(window.click("publishIntake"));
      QTRY_VERIFY(!window.flow.busy());
      QVERIFY(!window.flow.error().isEmpty());
      QVERIFY(window.flow.intake_preview());
      QVERIFY(!QFileInfo::exists(parent + "/missing"));
      // Commit retains the reviewed snapshot. Never re-read or write the external file.
      replace_file(source, "Changed externally after preview.");
      window.item("intakeProjectParent")->setProperty("text", parent);
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/import-preview.png")));
      QVERIFY(window.click("publishIntake"));
      QTRY_VERIFY(window.flow.intake_active());
      QTRY_VERIFY(!window.flow.busy());
      published = window.flow.location();
      QCOMPARE(published, parent + "/Imported (2)");
      QVERIFY(!window.flow.document());
      QVERIFY(!window.flow.autosaver());
      QCOMPARE(bytes(published + "/imports/original.fountain"), original);
      QCOMPARE(bytes(source), QByteArray("Changed externally after preview."));
      QCOMPARE(bytes(parent + "/Imported"), QByteArray("collision evidence"));
      metadata = bytes(published + "/imported-material.json");
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/import-reader.png")));
      QCOMPARE(SessionState(state).load()->directory, published);
      QVERIFY(window.click("intakeHome"));
      QTRY_VERIFY(!window.flow.busy());
      QVERIFY(!window.flow.intake_active());
      { ProjectSession unlocked(published); }
      window.item("openLocation")->setProperty("text", writing.directory);
      QVERIFY(window.click("openProject"));
      QTRY_VERIFY(window.flow.document());
      window.type("Separate writer ");
      QVERIFY(window.click("goHome"));
      QTRY_VERIFY(!window.flow.document());
      QTRY_VERIFY(!window.flow.busy());
      QCOMPARE(open_project(writing.directory).screenplay.elements.front().text, std::string("Separate writer Current writing."));
      QCOMPARE(bytes(published + "/imports/original.fountain"), original);
      QVERIFY(window.click("recentProject1"));
      QTRY_VERIFY(window.flow.intake_active());
      QTRY_VERIFY(!window.flow.busy());
      QCOMPARE(window.flow.location(), published);
    }
    {
      Window restarted(font_, state);
      QTRY_VERIFY(restarted.flow.intake_active());
      QTRY_VERIFY(!restarted.flow.busy());
      QCOMPARE(restarted.flow.location(), published);
      QVERIFY(!restarted.flow.document());
      QVERIFY(restarted.flow.intake_source().contains("/*omitted*/"));
      QVERIFY(restarted.click("intakeHome"));
      QTRY_VERIFY(!restarted.flow.busy());
    }
    QCOMPARE(bytes(published + "/imports/original.fountain"), original);
    QCOMPARE(bytes(published + "/imported-material.json"), metadata);
  }
  void cancelled_and_failed_imports_do_not_publish_or_replace_resume() {
    const auto state = path("intake-cancel-session");
    const auto source = path("candidate");
    replace_file(source, "Retain this source.");
    Window window(font_, state);
    QTRY_VERIFY(!window.flow.busy());
    window.flow.preview_import(source + "-missing");
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.error().isEmpty());
    QVERIFY(!window.flow.intake_preview());
    window.flow.preview_import(source);
    QTRY_VERIFY(window.flow.intake_preview());
    QVERIFY(window.click("intakeHome"));
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.intake_preview());
    QVERIFY(!window.flow.intake_active());
    QVERIFY(!SessionState(state).load());
    QVERIFY(ProjectLibrary(state + ".recents").load().empty());
    QCOMPARE(bytes(source), QByteArray("Retain this source."));
  }
  void damaged_intake_uses_existing_recovery_controls_and_stays_read_only() {
    const auto source = path("retained-source");
    replace_file(source, "A source. [[retained note]]\n");
    const auto intake = read_fountain_intake(source);
    const auto p = create_project(path("review-project"), {0, {{ElementType::action, ""}}}, intake.assets);
    const auto opened = open_project(p.directory);
    RecoveryStore store(p, opened.assets, p.directory + "/.recovery");
    const auto package = store.preserve(opened.screenplay, "reload", QDateTime::currentDateTimeUtc());
    replace_file(p.directory + "/imported-material.json", "damaged imported evidence");
    const auto state = path("recovered-review-session");
    Window window(font_, state, p.directory);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.intake_active());
    QVERIFY(!window.flow.document());
    QCOMPARE(window.flow.candidates().size(), 1);
    QVERIFY(window.click("recoveryChoice0"));
    const auto destination = path("recovered-review");
    window.item("recoveryDestination")->setProperty("text", destination);
    QVERIFY(window.click("recoverProject"));
    QTRY_VERIFY(window.flow.intake_active());
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.autosaver());
    QCOMPARE(bytes(destination + "/imports/original.fountain"), QByteArray("A source. [[retained note]]\n"));
    QCOMPARE(bytes(p.directory + "/imported-material.json"), QByteArray("damaged imported evidence"));
    QVERIFY(open_project(destination).project.import_review);
    Window competing(font_, path("competing-state"), destination);
    QTRY_VERIFY(!competing.flow.busy());
    QVERIFY(!competing.flow.intake_active());
    QVERIFY(competing.flow.error().contains("locked"));
    QCOMPARE(bytes(package.directory + "/imports/original.fountain"), QByteArray("A source. [[retained note]]\n"));
  }
  void explicit_fountain_preview_takes_precedence_over_remembered_project() {
    const auto p = project();
    const auto state = path("dispatch-state");
    SessionState(state).save({p.directory, p.id, {}, 0, 0});
    const auto state_before = bytes(state);
    const auto source = path("explicit") + ".fountain";
    replace_file(source, "Explicit preview source.");
    Window window(font_, state, {}, {}, source);
    QTRY_VERIFY(window.flow.intake_preview());
    QVERIFY(!window.flow.document());
    QCOMPARE(bytes(state), state_before);
    QCOMPARE(window.flow.intake_source(), QString("Explicit preview source."));
  }
  void changed_published_intake_cannot_activate_as_the_reviewed_snapshot() {
    const auto state = path("publication-session");
    const auto input = path("reviewed");
    const auto replacement = path("replacement");
    replace_file(input, "The reviewed original.");
    replace_file(replacement, "Different valid retained source.");
    const auto other = read_fountain_intake(replacement);
    const auto parent = path("publication-parent");
    QVERIFY(QDir().mkdir(parent));
    Window window(font_, state);
    QTRY_VERIFY(!window.flow.busy());
    window.flow.preview_import(input);
    QTRY_VERIFY(window.flow.intake_preview());
    window.flow.publish_import(parent, "Review");
    const auto published = parent + "/Review";
    // Hold GUI callbacks until the real worker publishes. Mutate the complete
    // package before safe-access activation, retaining its expected UUID.
    QElapsedTimer deadline; deadline.start();
    while (!QFileInfo::exists(published + "/project.json") && deadline.elapsed() < 5000) QThread::msleep(1);
    QVERIFY(QFileInfo::exists(published + "/project.json"));
    replace_file(published + "/imported-material.json", *other.assets.imported_material);
    replace_file(published + "/imports/original.fountain", *other.assets.original_fountain);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.intake_active());
    QVERIFY(!window.flow.document());
    QVERIFY(window.flow.error().contains("differs from the reviewed bytes"));
    QCOMPARE(window.flow.location(), published);
    QVERIFY(!SessionState(state).load());
    QVERIFY(ProjectLibrary(state + ".recents").load().empty());
    QCOMPARE(bytes(input), QByteArray("The reviewed original."));
    QCOMPARE(bytes(published + "/imports/original.fountain"), QByteArray("Different valid retained source."));
  }
};
QTEST_MAIN(ProjectAccessFlowTests)
#include "project_access_flow_tests.moc"
