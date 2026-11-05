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
