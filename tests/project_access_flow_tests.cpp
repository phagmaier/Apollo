#include "editor/project_access_flow.h"
#include "editor/autosave.h"
#include "editor/screenplay_layout.h"
#include "source/recovery.h"

#include <QDir>
#include <QFile>
#include <QQuickItem>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTextCursor>
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
  Window(const QFont& font, const QString& state, const QString& project = {})
      : flow(view, font, QStringLiteral(APOLLO_FONT_DIR), {}, state) {
    view.resize(1000, 1000);
    flow.start(project);
    view.show();
  }
  QQuickItem* item(const char* name) { return find_item(view.rootObject(), QString::fromLatin1(name)); }
  bool click(const char* name) {
    auto* button = item(name);
    if (!button || !button->isVisible() || !button->isEnabled()) return false;
    // ScrollView controls are real mouse targets, including those below the fold.
    auto* content = button->parentItem();
    while (content && !content->inherits("QQuickFlickable")) content = content->parentItem();
    if (content) {
      const auto y = button->mapToItem(content, QPointF()).y() + content->property("contentY").toReal();
      content->setProperty("contentY", qMax(0.0, y - 40));
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
};
QTEST_MAIN(ProjectAccessFlowTests)
#include "project_access_flow_tests.moc"
