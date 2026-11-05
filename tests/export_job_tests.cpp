#include "editor/fountain_export_job.h"
#include "tests/fountain_worker.h"
#include "editor/pdf_export_job.h"
#include "editor/screenplay_layout.h"
#include "fountain/import.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QQuickView>
#include "editor/editor_view.h"
#include "export/snapshot.h"
#include <QtTest>
#include <csignal>
#include <cerrno>

using namespace apollo;
using namespace apollo::editor;

class ExportJobTests : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    QTest::failOnWarning(QRegularExpression(".*"));
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
      QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  }
  void snapshot_and_busy_policy() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto document = screenplay_document(import_fountain("Title: Snapshot\n\nA **lamp** turns.\n"),
                                        courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), QStringLiteral(APOLLO_EXPORTER));
    QSignalSpy done(&job, &PdfExportJob::completed);
    const auto path = directory.filePath("snapshot.pdf");
    QFile previous(path);
    QVERIFY(previous.open(QIODevice::WriteOnly)); previous.write("old PDF"); previous.close();
    const auto permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup;
    QVERIFY(previous.setPermissions(permissions));
    QVERIFY(job.request(path, false));
    QVERIFY(job.busy());
    QVERIFY(!job.request(directory.filePath("rejected.pdf"), true));
    // Destroy the live document before the worker completes: only owned data
    // may cross the boundary. The title option belongs to the first request.
    QTextCursor cursor(document.get());
    cursor.select(QTextCursor::Document);
    cursor.insertText(QString::fromUtf8("Later unsupported 🚀 text."));
    document.reset();
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    QVERIFY(done.front().front().toBool());
    QVERIFY(!job.busy());
    QVERIFY(QFile::exists(path));
    QProcess extracted;
    extracted.start("pdftotext", {path, "-"});
    QVERIFY(extracted.waitForFinished(5000));
    QCOMPARE(extracted.exitCode(), 0);
    QCOMPARE(extracted.readAllStandardOutput().simplified(), QByteArray("A lamp turns."));
    QCOMPARE(QFileInfo(path).permissions() & (QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther), permissions);
    QVERIFY(QDir(directory.path()).entryList({".apollo-export-*"}, QDir::Files | QDir::Hidden).isEmpty());
    QVERIFY(!QFile::exists(directory.filePath("rejected.pdf")));
    QVERIFY(job.total_ms() >= job.capture_ms());
  }

  void failure_and_retry_data() {
    QTest::addColumn<QString>("worker");
    QTest::addColumn<QString>("fonts");
    QTest::addColumn<QString>("text");
    QTest::newRow("missing worker") << QStringLiteral("/nonexistent/apollo-exporter") << QStringLiteral(APOLLO_FONT_DIR) << "A lamp.";
    QTest::newRow("worker exit") << QStringLiteral("/usr/bin/false") << QStringLiteral(APOLLO_FONT_DIR) << "A lamp.";
    QTest::newRow("missing fonts") << QStringLiteral(APOLLO_EXPORTER) << QStringLiteral("/nonexistent/fonts") << "A lamp.";
    QTest::newRow("unsupported glyph") << QStringLiteral(APOLLO_EXPORTER) << QStringLiteral(APOLLO_FONT_DIR) << QString::fromUtf8("A 🚀.");
  }
  void failure_and_retry() {
    QFETCH(QString, worker); QFETCH(QString, fonts); QFETCH(QString, text);
    QTemporaryDir directory;
    const auto path = directory.filePath("existing.pdf");
    QFile original(path);
    QVERIFY(original.open(QIODevice::WriteOnly));
    QCOMPARE(original.write("existing destination"), 20);
    original.close();
    auto document = screenplay_document({{ElementType::action, text.toStdString()}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), fonts, worker);
    QSignalSpy done(&job, &PdfExportJob::completed);
    for (int attempt = 0; attempt < 2; ++attempt) {
      QVERIFY(job.request(path));
      QTRY_COMPARE_WITH_TIMEOUT(done.size(), attempt + 1, 15000);
      QVERIFY(!done.back().front().toBool());
      QVERIFY(!job.busy());
      QVERIFY(job.status().contains("failed", Qt::CaseInsensitive));
      QVERIFY(original.open(QIODevice::ReadOnly));
      QCOMPARE(original.readAll(), QByteArray("existing destination"));
      original.close();
      QVERIFY(QDir(directory.path()).entryList({".apollo-export-*"}, QDir::Files | QDir::Hidden).isEmpty());
    }
  }

  void fountain_destruction_bounds_stalled_work() {
    QTemporaryDir directory;
    QFile output(directory.filePath("existing.fountain"));
    QVERIFY(output.open(QIODevice::WriteOnly)); output.write("previous Fountain"); output.close();
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    QElapsedTimer elapsed;
    qint64 pid = 0;
    {
      FountainExportJob job(document.get(), directory.filePath("Project"));
      HeldFountainWorker held(job, directory.filePath("held-worker"));
      QVERIFY(job.request(output.fileName()));
      QTRY_VERIFY(held.stopped());
      pid = job.worker_pid();
      elapsed.start();
    }
    QVERIFY2(elapsed.elapsed() < 1500, "Fountain teardown must not wait for stalled work");
    errno = 0; QCOMPARE(::kill(static_cast<pid_t>(pid), 0), -1); QCOMPARE(errno, ESRCH);
    QVERIFY(QDir(directory.path()).entryList({".apollo-fountain-*"}, QDir::Files | QDir::Hidden).isEmpty());
    QCoreApplication::processEvents();
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll(), QByteArray("previous Fountain"));
  }

  void fountain_snapshot_busy_and_retry() {
    QTemporaryDir directory;
    auto document = screenplay_document({{ElementType::title, "SNAPSHOT"},
        {ElementType::heading, "int. room - day"}, {ElementType::action, "A lamp turns.", false, {{2, 6, Style::bold}}}},
        courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    FountainExportJob job(document.get(), directory.filePath("Project"));
    HeldFountainWorker held(job, directory.filePath("held-worker"));
    QFile output(directory.filePath("snapshot.fountain"));
    QVERIFY(output.open(QIODevice::WriteOnly)); output.write("previous Fountain"); output.close();
    const auto permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup;
    QVERIFY(output.setPermissions(permissions));
    QSignalSpy done(&job, &FountainExportJob::completed);
    QVERIFY(job.request(output.fileName(), false));
    const auto capture = job.status().section(" from ", 1).section(" to ", 0, 0);
    QVERIFY(QRegularExpression("^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}$").match(capture).hasMatch());
    QTRY_VERIFY(held.stopped());
    QVERIFY(!job.request(directory.filePath("rejected.fountain"), true));
    document->setPlainText(QString::fromUtf8("Later unsupported 🚀 text."));
    document.reset();
    QVERIFY(held.resume());
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(done.front().front().toBool());
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll(), QByteArray("INT. ROOM - DAY\n\nA **lamp** turns.\n")); output.close();
    QCOMPARE(QFileInfo(output).permissions() & (QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther), permissions);
    QVERIFY(job.status().contains(capture));
    QVERIFY(job.status().contains("Later edits are not included."));
    QVERIFY(job.status().contains("Title page left out by choice."));
    QVERIFY(job.status().contains("typed in lowercase"));
    QVERIFY(!QFile::exists(directory.filePath("rejected.fountain")));
    QVERIFY(QDir(directory.path()).entryList({".apollo-fountain-*"}, QDir::Files | QDir::Hidden).isEmpty());
  }

  void fountain_cancellation_failure_and_retry_data() {
    QTest::addColumn<QString>("mode");
    for (const auto* mode : {"cancel-start", "cancel-staged", "crash-staged", "failed-exit", "bad-response", "missing-worker", "publish-failure"})
      QTest::newRow(mode) << QString::fromLatin1(mode);
  }
  void fountain_cancellation_failure_and_retry() {
    QFETCH(QString, mode);
    QTemporaryDir directory;
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    FountainExportJob job(document.get(), directory.filePath("Project"));
    HeldFountainWorker held(job, directory.filePath("worker"));
    if (mode == "missing-worker") job.set_worker_program(directory.filePath("missing-worker"));
    if (mode == "failed-exit" || mode == "bad-response") {
      QFile wrapper(directory.filePath("worker"));
      QVERIFY(wrapper.open(QIODevice::WriteOnly));
      // A bad helper may write the staging file; without a successful protocol
      // response and clean exit it still cannot replace the existing output.
      wrapper.write("#!/bin/sh\necho partial > \"$1\"\n");
      wrapper.write(mode == "failed-exit" ? "exit 1\n" : "echo garbage\nexit 0\n");
    }
    QFile output(directory.filePath("existing.fountain"));
    QVERIFY(output.open(QIODevice::WriteOnly)); output.write("previous Fountain"); output.close();
    QSignalSpy done(&job, &FountainExportJob::completed);
    QVERIFY(job.request(output.fileName()));
    if (mode.endsWith("staged") || mode == "publish-failure") QTRY_VERIFY(held.stopped());
    if (mode == "publish-failure") {
      // Destination changed after capture: rename must fail without publishing
      // or losing the prior file moved safely aside by its owner.
      QVERIFY(output.rename(directory.filePath("previous.fountain")));
      QVERIFY(QDir().mkdir(directory.filePath("existing.fountain")));
      QVERIFY(held.resume());
    }
    const auto pid = job.worker_pid();
    if (mode.startsWith("cancel")) { job.cancel(); job.cancel(); }
    if (mode == "crash-staged") QVERIFY(::kill(static_cast<pid_t>(pid), SIGKILL) == 0);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 2000);
    QVERIFY(!done.front().front().toBool());
    QVERIFY(!job.busy()); QVERIFY(!job.attention()); QVERIFY(!job.cancelling());
    QVERIFY(job.status().contains(mode.startsWith("cancel") ? "cancelled" : "failed"));
    if (pid) { errno = 0; QCOMPARE(::kill(static_cast<pid_t>(pid), 0), -1); QCOMPARE(errno, ESRCH); }
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll(), QByteArray("previous Fountain")); output.close();
    QVERIFY(QDir(directory.path()).entryList({".apollo-fountain-*"}, QDir::Files | QDir::Hidden).isEmpty());
    if (mode == "publish-failure") {
      QVERIFY(QDir().rmdir(directory.filePath("existing.fountain")));
      QVERIFY(output.rename(directory.filePath("existing.fountain")));
    }
    job.set_worker_program({});
    QVERIFY(job.request(output.fileName()));
    QTRY_COMPARE(done.size(), 2);
    QVERIFY(done.back().front().toBool());
    QVERIFY(output.open(QIODevice::ReadOnly)); QCOMPARE(output.readAll(), QByteArray("A lamp.\n"));
  }

  void fountain_watchdog_and_controls_keep_waiting_then_cancel() {
    QTemporaryDir directory;
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    FountainExportJob job(document.get(), directory.filePath("Project"));
    HeldFountainWorker held(job, directory.filePath("worker"));
    job.set_watchdog_interval(500);
    QQuickView view;
    auto* editor = show_editor(view, *document, nullptr, directory.filePath("output.pdf"), nullptr, &job);
    view.resize(1000, 800); view.show(); view.requestActivate(); editor->forceActiveFocus();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    auto click = [&](const char* name) {
      QTest::qWait(30);
      auto* item = view.rootObject()->findChild<QQuickItem*>(QString::fromLatin1(name));
      if (!item || !item->isVisible() || !item->isEnabled()) return false;
      QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, item->mapToScene(item->boundingRect().center()).toPoint());
      return true;
    };
    QSignalSpy done(&job, &FountainExportJob::completed);
    const auto path = directory.filePath("cancelled.fountain");
    QVERIFY(job.request(path));
    auto* panel = view.rootObject()->findChild<QObject*>("fountainWaitPanel");
    QVERIFY(panel); QTRY_VERIFY(panel->property("opened").toBool());
    QVERIFY(click("fountainKeepWaiting"));
    QTRY_VERIFY(!panel->property("visible").toBool());
    QVERIFY(job.busy()); QVERIFY(!job.attention()); QVERIFY(done.isEmpty());
    QTest::keyClick(&view, Qt::Key_End, Qt::ControlModifier);
    QTest::keyClick(&view, Qt::Key_X);
    QVERIFY(document->toPlainText().endsWith('x'));
    QTRY_VERIFY(panel->property("opened").toBool());
    QVERIFY(click("fountainWaitCancel"));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 2000);
    QVERIFY(!done.front().front().toBool()); QVERIFY(!QFile::exists(path));
    QTRY_VERIFY(!panel->property("visible").toBool());
    job.set_watchdog_interval(30000);
    QVERIFY(job.request(path));
    // Later ineligible writing can produce a long disclosure. It must not
    // push cancellation of the already accepted snapshot outside the window.
    document->setPlainText(QString::fromUtf8("Later unsupported 🚀 text.\n").repeated(12));
    QVERIFY(!job.check(true).isEmpty());
    view.resize(1000, 600);
    QVERIFY(click("exportButton"));
    auto* options = view.rootObject()->findChild<QObject*>("exportPanel");
    QTRY_VERIFY(options->property("opened").toBool());
    QVERIFY(click("exportFormatFountain"));
    QTest::qWait(30);
    auto* cancel = view.rootObject()->findChild<QQuickItem*>("fountainCancel");
    QVERIFY(cancel);
    QVERIFY2(cancel->mapToScene(QPointF(0, cancel->height())).y() <= view.height(),
             "Cancel Export must remain reachable when later writing is ineligible");
    QVERIFY(click("fountainCancel"));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 2000);
    QVERIFY(!done.back().front().toBool());
  }

  void destruction_bounds_a_stalled_worker_and_preserves_destination() {
    QTemporaryDir directory;
    QFile worker(directory.filePath("worker"));
    QVERIFY(worker.open(QIODevice::WriteOnly));
    worker.write("#!/bin/sh\nexec sleep 3\n"); worker.close();
    QVERIFY(worker.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QFile pdf(directory.filePath("closed.pdf"));
    QVERIFY(pdf.open(QIODevice::WriteOnly)); pdf.write("previous PDF"); pdf.close();
    auto document = screenplay_document(std::vector<Element>(1000, {ElementType::action, std::string(120, 'a') + "."}),
                                        courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    qint64 pid = 0;
    QElapsedTimer elapsed;
    {
      PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), worker.fileName());
      QVERIFY(job.request(pdf.fileName()));
      QTRY_VERIFY(job.worker_pid() > 0);
      pid = job.worker_pid();
      elapsed.start();
    }
    QVERIFY2(elapsed.elapsed() < 1500, "PDF teardown must not wait for a stalled helper");
    QVERIFY(::kill(static_cast<pid_t>(pid), 0) != 0);
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    QCOMPARE(pdf.readAll(), QByteArray("previous PDF"));
    QVERIFY(QDir(directory.path()).entryList({".apollo-export-*"}, QDir::Files | QDir::Hidden).isEmpty());
    QCoreApplication::processEvents(); // no callback into the destroyed job
  }

  void cancellation_preserves_output_and_can_retry_data() {
    QTest::addColumn<bool>("after_painting");
    QTest::newRow("blocked stdin") << false;
    QTest::newRow("completed staged PDF") << true;
  }
  void cancellation_preserves_output_and_can_retry() {
    QFETCH(bool, after_painting);
    QTemporaryDir directory;
    const auto ready = directory.filePath("ready");
    QFile wrapper(directory.filePath("worker"));
    QVERIFY(wrapper.open(QIODevice::WriteOnly));
    // First invocation stops even after writing a full PDF. Retry then execs
    // the actual helper. No child/grandchild is left behind on cancellation.
    const auto quote = [](QString path) { return "'" + path.toUtf8().replace("'", "'\\''") + "'"; };
    const auto exporter = quote(QStringLiteral(APOLLO_EXPORTER));
    wrapper.write("#!/bin/sh\nif [ ! -e " + quote(ready) + " ]; then\n" +
                  (after_painting ? exporter + " \"$@\" || exit 1\n" : QByteArray()) +
                  "touch " + quote(ready) + "\nkill -STOP $$\nfi\nexec " + exporter + " \"$@\"\n");
    wrapper.close();
    QVERIFY(wrapper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QFile output(directory.filePath("existing.pdf"));
    QVERIFY(output.open(QIODevice::WriteOnly)); output.write("previous PDF"); output.close();
    auto document = screenplay_document(std::vector<Element>(1000, {ElementType::action, std::string(120, 'a') + "."}),
                                        courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), wrapper.fileName());
    QSignalSpy done(&job, &PdfExportJob::completed);
    QVERIFY(job.request(output.fileName()));
    // Cancel while starting must work as well as cancelling a stopped process.
    if (after_painting) QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(ready), 10000);
    const auto pid = job.worker_pid();
    job.cancel(); job.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 2000);
    QVERIFY(!done.front().front().toBool());
    QVERIFY(!job.busy()); QVERIFY(!job.attention()); QVERIFY(!job.cancelling());
    QVERIFY(job.status().contains("cancelled"));
    if (pid) { errno = 0; QCOMPARE(::kill(static_cast<pid_t>(pid), 0), -1); QCOMPARE(errno, ESRCH); }
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll(), QByteArray("previous PDF")); output.close();
    QVERIFY(QDir(directory.path()).entryList({".apollo-export-*"}, QDir::Files | QDir::Hidden).isEmpty());
    // The first cancellation may precede execution of the wrapper entirely.
    QFile marker(ready); QVERIFY(marker.open(QIODevice::WriteOnly)); marker.close();
    QVERIFY(job.request(output.fileName()));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 15000);
    QVERIFY(done.back().front().toBool());
    QVERIFY(output.open(QIODevice::ReadOnly)); QVERIFY(output.readAll().startsWith("%PDF-"));
  }

  void watchdog_and_real_controls_keep_waiting_then_cancel() {
    QTemporaryDir directory;
    QFile wrapper(directory.filePath("worker"));
    QVERIFY(wrapper.open(QIODevice::WriteOnly));
    wrapper.write("#!/bin/sh\nkill -STOP $$\n"); wrapper.close();
    QVERIFY(wrapper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), wrapper.fileName());
    job.set_watchdog_interval(200);
    QQuickView view;
    const auto path = directory.filePath("cancelled.pdf");
    auto* editor = show_editor(view, *document, &job, path);
    view.resize(1000, 800); view.show(); view.requestActivate(); editor->forceActiveFocus();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    auto click = [&](const char* name) {
      QTest::qWait(30); // Polish Popup geometry before pointer input.
      auto* item = view.rootObject()->findChild<QQuickItem*>(QString::fromLatin1(name));
      if (!item || !item->isVisible() || !item->isEnabled()) return false;
      QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, item->mapToScene(item->boundingRect().center()).toPoint());
      return true;
    };
    QSignalSpy done(&job, &PdfExportJob::completed);
    QTest::keyClick(&view, Qt::Key_E, Qt::ControlModifier);
    QTRY_VERIFY(job.attention());
    auto* panel = view.rootObject()->findChild<QObject*>("pdfWaitPanel");
    QVERIFY(panel); QTRY_VERIFY(panel->property("opened").toBool());
    // Re-arm with a generous interval while the test types real keys.
    job.set_watchdog_interval(500);
    QVERIFY(click("pdfKeepWaiting"));
    QTRY_VERIFY(!panel->property("visible").toBool());
    QVERIFY(job.busy()); QVERIFY(!job.attention()); QVERIFY(done.isEmpty());
    QTest::keyClick(&view, Qt::Key_End, Qt::ControlModifier);
    QTest::keyClick(&view, Qt::Key_X);
    QVERIFY(document->toPlainText().endsWith('x'));
    QTRY_VERIFY(panel->property("opened").toBool());
    QVERIFY(click("pdfWaitCancel"));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 2000);
    QVERIFY(!done.front().front().toBool()); QVERIFY(!QFile::exists(path));
    QTRY_VERIFY(!panel->property("visible").toBool());
    // Ordinary Export options also allow cancelling without the watchdog.
    job.set_watchdog_interval(30000);
    QTest::keyClick(&view, Qt::Key_E, Qt::ControlModifier);
    QVERIFY(job.busy());
    QVERIFY(click("exportButton"));
    auto* options = view.rootObject()->findChild<QObject*>("exportPanel");
    QTRY_VERIFY(options->property("opened").toBool());
    QVERIFY(click("pdfCancel"));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 2000);
    QVERIFY(!done.back().front().toBool());
  }

  void crash_preserves_destination_and_can_retry() {
    QTemporaryDir directory;
    const auto path = directory.filePath("existing.pdf");
    QFile pdf(path);
    QVERIFY(pdf.open(QIODevice::WriteOnly));
    pdf.write("previous PDF"); pdf.close();
    auto document = screenplay_document(std::vector<Element>(2000, {ElementType::action, std::string(120, 'a') + "."}),
                                        courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), QStringLiteral(APOLLO_EXPORTER));
    QSignalSpy done(&job, &PdfExportJob::completed);
    QVERIFY(job.request(path));
    QTRY_VERIFY_WITH_TIMEOUT(job.worker_pid() > 0, 1000);
    QVERIFY(::kill(static_cast<pid_t>(job.worker_pid()), SIGKILL) == 0);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    QVERIFY(!done.front().front().toBool());
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    QCOMPARE(pdf.readAll(), QByteArray("previous PDF")); pdf.close();
    document->setPlainText("A lamp.");
    QVERIFY(job.request(path));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 15000);
    QVERIFY(done.back().front().toBool());
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    QVERIFY(pdf.readAll().endsWith("%%EOF\n"));
  }

  void invalid_destination_and_source_protection() {
    QTemporaryDir directory;
    const auto source = directory.filePath("script.fountain");
    QFile input(source);
    QVERIFY(input.open(QIODevice::WriteOnly)); input.write("A lamp.\n"); input.close();
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), QStringLiteral(APOLLO_EXPORTER), source);
    QVERIFY(!job.request(source));
    QVERIFY(job.status().contains("must differ"));
    const auto alias = directory.filePath("alias.pdf");
    QVERIFY(QFile::link(source, alias));
    QVERIFY(!job.request(alias));
    QVERIFY(!job.request(""));
    QSignalSpy done(&job, &PdfExportJob::completed);
    QVERIFY(!job.request(directory.filePath("missing/out.pdf")));
    QVERIFY(job.status().contains("failed"));
    // A final replacement failure after successful painting is reported too.
    QVERIFY(job.request(directory.path()));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    QVERIFY(!done.front().front().toBool());
    QVERIFY(input.open(QIODevice::ReadOnly)); QCOMPARE(input.readAll(), QByteArray("A lamp.\n"));
  }

  void project_destinations_are_protected() {
    QTemporaryDir directory;
    const auto project = directory.filePath("Project");
    QVERIFY(QDir().mkpath(project + "/imports"));
    QFile source(project + "/screenplay.json");
    QVERIFY(source.open(QIODevice::WriteOnly)); source.write("retained writing"); source.close();
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), QStringLiteral(APOLLO_EXPORTER), project);
    QVERIFY(!job.request(source.fileName()));
    QVERIFY(job.status().contains("outside the project folder"));
    QVERIFY(!job.request(project + "/imports/new.pdf"));
    const auto alias = directory.filePath("alias.pdf");
    QVERIFY(QFile::link(source.fileName(), alias));
    QVERIFY(!job.request(alias));
    const auto folder_alias = directory.filePath("project-alias");
    QVERIFY(QFile::link(project, folder_alias));
    QVERIFY(!job.request(folder_alias + "/new.pdf"));
    QFile outside(directory.filePath("outside.pdf"));
    QVERIFY(outside.open(QIODevice::WriteOnly)); outside.write("outside evidence"); outside.close();
    QVERIFY(QFile::link(outside.fileName(), project + "/linked.pdf"));
    QVERIFY(!job.request(folder_alias + "/linked.pdf"));
    QVERIFY(outside.open(QIODevice::ReadOnly)); QCOMPARE(outside.readAll(), QByteArray("outside evidence"));
    QVERIFY(source.open(QIODevice::ReadOnly)); QCOMPARE(source.readAll(), QByteArray("retained writing"));
    QVERIFY(!QFile::exists(project + "/imports/new.pdf"));
    QVERIFY(QDir(project).entryList({".apollo-export-*"}, QDir::Files | QDir::Hidden).isEmpty());
  }

  void fountain_project_and_original_aliases_are_protected() {
    QTemporaryDir directory;
    const auto project = directory.filePath("Project");
    QVERIFY(QDir().mkpath(project + "/imports"));
    QFile source(project + "/screenplay.json");
    QVERIFY(source.open(QIODevice::WriteOnly)); source.write("retained writing"); source.close();
    QFile original(directory.filePath("original.fountain"));
    QVERIFY(original.open(QIODevice::WriteOnly)); original.write("original writing"); original.close();
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    FountainExportJob job(document.get(), project, {}, {original.fileName()});
    QVERIFY(!job.request(source.fileName()));
    QVERIFY(!job.request(project + "/imports/new.fountain"));
    QVERIFY(QFile::link(source.fileName(), directory.filePath("alias.fountain")));
    QVERIFY(!job.request(directory.filePath("alias.fountain")));
    QVERIFY(QFile::link(project, directory.filePath("project-alias")));
    QVERIFY(!job.request(directory.filePath("project-alias/new.fountain")));
    QFile outside(directory.filePath("outside.fountain"));
    QVERIFY(outside.open(QIODevice::WriteOnly)); outside.write("outside evidence"); outside.close();
    QVERIFY(QFile::link(outside.fileName(), project + "/linked.fountain"));
    QVERIFY(!job.request(directory.filePath("project-alias/linked.fountain")));
    QVERIFY(!job.request(project + "/linked.fountain"));
    QVERIFY(job.status().contains("outside the project folder"));
    QVERIFY(QFile::link(original.fileName(), directory.filePath("original-alias.fountain")));
    QVERIFY(!job.request(directory.filePath("original-alias.fountain")));
    QVERIFY(job.status().contains("never replaces it"));
    QVERIFY(outside.open(QIODevice::ReadOnly)); QCOMPARE(outside.readAll(), QByteArray("outside evidence"));
    QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), QByteArray("original writing"));
    QVERIFY(source.open(QIODevice::ReadOnly)); QCOMPARE(source.readAll(), QByteArray("retained writing"));
    QVERIFY(!QFile::exists(project + "/imports/new.fountain"));
  }

  void worker_fails_after_writing_complete_pdf() {
    QTemporaryDir directory;
    QFile wrapper(directory.filePath("worker"));
    QVERIFY(wrapper.open(QIODevice::WriteOnly));
    const auto exporter_path = QByteArray(APOLLO_EXPORTER).replace("'", "'\\''");
    wrapper.write("#!/bin/sh\n'" + exporter_path + "' \"$@\"\nexit 1\n");
    wrapper.close();
    QVERIFY(wrapper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QFile existing(directory.filePath("existing.pdf"));
    QVERIFY(existing.open(QIODevice::WriteOnly)); existing.write("previous PDF"); existing.close();
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), wrapper.fileName());
    QSignalSpy done(&job, &PdfExportJob::completed);
    QVERIFY(job.request(existing.fileName()));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    QVERIFY(!done.front().front().toBool());
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("previous PDF"));
  }

  void editor_export_shortcut_and_feedback() {
    QTemporaryDir directory;
    auto document = screenplay_document({{ElementType::action, "A lamp."}}, courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), QStringLiteral(APOLLO_EXPORTER));
    QQuickView view;
    const auto path = directory.filePath("from-editor.pdf");
    auto* editor = show_editor(view, *document, &job, path);
    view.resize(1000, 800); view.show(); view.requestActivate(); editor->forceActiveFocus();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    auto* button = view.rootObject()->findChild<QQuickItem*>("exportButton");
    auto* panel = view.rootObject()->findChild<QObject*>("exportPanel");
    QVERIFY(button && panel);
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, button->mapToScene(button->boundingRect().center()).toPoint());
    QTRY_VERIFY(panel->property("visible").toBool());
    QTest::keyClick(&view, Qt::Key_Escape);
    QTRY_VERIFY(!panel->property("visible").toBool());
    QVERIFY(editor->hasActiveFocus());
    QSignalSpy done(&job, &PdfExportJob::completed);
    QTest::keyClick(&view, Qt::Key_E, Qt::ControlModifier);
    QVERIFY(job.busy());
    QVERIFY(job.status().startsWith("Exporting"));
    QVERIFY(editor->hasActiveFocus());
    QTest::keyClick(&view, Qt::Key_End, Qt::ControlModifier);
    QTest::keyClick(&view, Qt::Key_X);
    QVERIFY(document->toPlainText().endsWith('x'));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    QVERIFY(done.front().front().toBool());
    QVERIFY(job.status().contains(path));
    QVERIFY(job.status().startsWith("PDF snapshot exported"));
    QVERIFY(job.status().contains("captured"));
    // A requested close also completes normally when the helper finishes;
    // the prompt must not require cancellation of an otherwise healthy export.
    QVERIFY(job.request(path));
    view.close();
    QVERIFY(view.isVisible());
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 15000);
    QVERIFY(done.back().front().toBool());
    QTRY_VERIFY(!view.isVisible());
    QVERIFY(!job.attention());
  }

  void malformed_worker_payload() {
    // Reject a truncated message, unknown profile/version and out-of-bounds
    // emphasis before any PDF is opened; don't treat a partial decode as valid.
    const auto valid = encode_export_snapshot({{{ElementType::action, "A lamp."}}, true});
    auto rejected = [](const QByteArray& bytes) {
      try { decode_export_snapshot(bytes); return false; } catch (const std::exception&) { return true; }
    };
    QVERIFY(rejected(valid.first(valid.size() - 1)));
    QVERIFY(rejected(valid + QByteArray("trailing data")));
    auto version = valid; version.replace("snapshot v1", "snapshot v2");
    QVERIFY(rejected(version));
    QVERIFY(rejected(encode_export_snapshot({{{ElementType::action, "A lamp.", false, {{0, 999, Style::bold}}}}, true})));
  }
};

QTEST_MAIN(ExportJobTests)
#include "export_job_tests.moc"
