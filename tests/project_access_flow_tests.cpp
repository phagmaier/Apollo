#include "editor/project_access_flow.h"
#include "editor/autosave.h"
#include "editor/pdf_export_job.h"
#include "editor/fountain_export_job.h"
#include "editor/screenplay_layout.h"
#include "editor/editor_commands.h"
#include "source/recovery.h"
#include "source/fountain_conversion.h"
#include "source/fountain_working.h"
#include "source/fountain_export.h"
#include <QJsonDocument>
#include <QJsonObject>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickView>
#include <QRawFont>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QSignalSpy>
#include <csignal>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <QTextCursor>
#include <QTextBlock>
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
// The glyphs a block draws are those of the given text, from its start.
bool draws(const QFont& font, const QTextBlock& block, const QString& text) {
  QList<quint32> drawn;
  for (const auto& run : block.layout()->glyphRuns(0, static_cast<int>(text.size()))) drawn += run.glyphIndexes();
  return drawn == QRawFont::fromFont(font).glyphIndexesForString(text);
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
    return find_item(view.contentItem(), QString::fromLatin1(name));
  }
  void type(const QString& text) {
    for (const auto ch : text) {
      QKeyEvent press(QEvent::KeyPress, ch.toUpper().unicode(), Qt::NoModifier, QString(ch));
      QKeyEvent release(QEvent::KeyRelease, ch.toUpper().unicode(), Qt::NoModifier, QString(ch));
      QCoreApplication::sendEvent(&view, &press);
      QCoreApplication::sendEvent(&view, &release);
    }
  }
  bool popup_open(const char* name) {
    const auto* popup = view.rootObject()->findChild<QObject*>(QString::fromLatin1(name));
    return popup && popup->property("opened").toBool();
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
  void title_form_apply_cancel_undo_reopen_and_export() {
    const auto requested = qEnvironmentVariable("APOLLO_TITLE_OUTPUT");
    const auto output_root = requested.isEmpty() ? path("title-artifacts") : requested;
    QVERIFY(QDir().mkpath(output_root));
    const auto p = create_project(output_root + "/project", {5, {
        {ElementType::action, "Current writing.", false, {}, "first"},
        {ElementType::action, "Second paragraph.", false, {}, "second"}}});
    const auto state = path("title-state");
    const auto output = output_root + "/captured.fountain";
    const auto body_output = output_root + "/body.fountain";
    const auto pdf = output_root + "/body.pdf";
    const QByteArray expected("Title: THE SIGNAL\nCredit: Written by\nAuthor: June Vale\n"
                              "Source: Based on a radio play\nContact: june@example.test\nDraft date: October 2026\n\n"
                              "Current writing.\n\nSecond paragraph.\n");
    const auto initial = open_project(p.directory).screenplay;
    {
      Window window(font_, state, p.directory);
      QTRY_VERIFY(window.flow.editor() && window.flow.editor()->hasActiveFocus());
      const auto fallback_title = window.item("documentTitle")->property("text").toString();
      QVERIFY(window.click("titlePageButton"));
      QTRY_VERIFY(window.popup_open("titlePagePanel"));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/title-form-open.png")));
      auto fill = [&](const char* name, const QString& text) {
        auto* field = window.item(name);
        if (!field) return false;
        field->forceActiveFocus();
        QMetaObject::invokeMethod(field, "selectAll");
        if (text.isEmpty()) QTest::keyClick(&window.view, Qt::Key_Backspace);
        else window.type(text);
        return true;
      };
      QVERIFY(fill("titleField_title_0", "Cancelled title"));
      QVERIFY(window.click("titleCancel"));
      QCOMPARE(open_project(p.directory).screenplay, initial);
      QVERIFY(!window.flow.document()->isUndoAvailable());
      QTest::keyClick(&window.view, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
      QTRY_VERIFY(window.popup_open("titlePagePanel"));
      QCOMPARE(window.item("titleField_title_0")->property("text").toString(), QString());
      QVERIFY(fill("titleField_title_0", "THE SIGNAL"));
      QVERIFY(fill("titleField_credit_1", "Written by"));
      QVERIFY(fill("titleField_author_2", "June Vale"));
      QVERIFY(fill("titleField_source_3", "Based on a radio play"));
      QVERIFY(fill("titleField_contact_4", "june@example.test"));
      QVERIFY(fill("titleField_draftDate_5", "October 2026"));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/title-form.png")));
      QVERIFY(window.click("titleApply"));
      QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
      QCOMPARE(window.item("documentTitle")->property("text").toString(), QString("THE SIGNAL"));
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      auto* pdf_job = qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>());
      QVERIFY(job && pdf_job);
      QCOMPARE(export_fountain(document_elements(*window.flow.document()), true).bytes, expected);
      const auto authored = canonical(Screenplay{0, document_elements(*window.flow.document())});
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(canonical(Screenplay{initial.revision, document_elements(*window.flow.document())}), initial);
      QCOMPARE(window.item("documentTitle")->property("text").toString(), fallback_title);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), authored);
      QCOMPARE(window.item("documentTitle")->property("text").toString(), QString("THE SIGNAL"));
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      auto saved = open_project(p.directory).screenplay;
      saved.revision = 0;
      QCOMPARE(saved, authored);
      // Capture before the next form Apply. Later title edits must not enter
      // the accepted worker snapshot, even though autosave includes them.
      std::mutex mutex;
      std::condition_variable condition;
      bool released = false;
      std::atomic<bool> entered = false;
      job->set_worker_hook([&] {
        std::unique_lock lock(mutex);
        entered = true;
        condition.wait(lock, [&] { return released; });
      });
      const auto release = qScopeGuard([&] {
        { std::lock_guard lock(mutex); released = true; }
        condition.notify_all();
      });
      QVERIFY(job->request(output));
      QTRY_VERIFY(entered.load());
      QVERIFY(window.click("titlePageButton"));
      QVERIFY(fill("titleField_title_0", "THE LATER SIGNAL"));
      QVERIFY(window.click("titleApply"));
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      { std::lock_guard lock(mutex); released = true; }
      condition.notify_all();
      QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(output), expected);
      job->set_worker_hook({});
      // Clearing an existing field preserves the blank in source, rather
      // than silently removing imported/authored metadata. Omission bypasses
      // only title checks, never body eligibility.
      QVERIFY(window.click("titlePageButton"));
      QVERIFY(fill("titleField_title_0", ""));
      QVERIFY(window.click("titleApply"));
      QCOMPARE(window.item("documentTitle")->property("text").toString(), fallback_title);
      QVERIFY(!job->check(true).isEmpty());
      QVERIFY(job->check(false).isEmpty());
      QVERIFY(!job->request(output, true));
      QCOMPARE(bytes(output), expected);
      QVERIFY(window.click("exportButton"));
      QTest::keyClick(&window.view, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
      QVERIFY(!window.popup_open("titlePagePanel"));
      QVERIFY(window.click("exportFormatFountain"));
      QVERIFY(window.click("includeTitle"));
      window.item("fountainDestination")->setProperty("text", body_output);
      QVERIFY(window.click("exportConfirm"));
      QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(body_output), QByteArray("Current writing.\n\nSecond paragraph.\n"));
      // The same shared checkbox is retained when changing export formats.
      QVERIFY(window.click("exportButton"));
      QVERIFY(window.click("exportFormatPdf"));
      QVERIFY(!window.item("includeTitle")->property("checked").toBool());
      window.item("exportDestination")->setProperty("text", pdf);
      QSignalSpy pdf_completed(pdf_job, &PdfExportJob::completed);
      QVERIFY(window.click("exportConfirm"));
      QTRY_VERIFY(!pdf_job->busy());
      QCOMPARE(pdf_completed.size(), 1);
      QVERIFY(pdf_completed[0][0].toBool());
      const auto previous_pdf = bytes(pdf);
      QVERIFY(window.click("titlePageButton"));
      QVERIFY(fill("titleField_title_0", QString(90, 'X')));
      QVERIFY(window.click("titleApply"));
      QVERIFY(pdf_job->request(pdf, true));
      QTRY_VERIFY(!pdf_job->busy());
      QCOMPARE(pdf_completed.size(), 2);
      QVERIFY(!pdf_completed[1][0].toBool());
      QCOMPARE(bytes(pdf), previous_pdf);
      QVERIFY(pdf_job->request(pdf, false));
      QTRY_VERIFY(!pdf_job->busy());
      QCOMPARE(pdf_completed.size(), 3);
      QVERIFY(pdf_completed[2][0].toBool());
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      // Restore the authored title using the document's undo, not the form.
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
    }
    {
      Window window(font_, state, p.directory);
      QTRY_VERIFY(window.flow.editor() && window.flow.editor()->hasActiveFocus());
      QVERIFY(window.click("titlePageButton"));
      QTRY_VERIFY(window.popup_open("titlePagePanel"));
      QCOMPARE(window.item("titleField_title_0")->property("text").toString(), QString("THE LATER SIGNAL"));
      QCOMPARE(window.item("titleField_contact_4")->property("text").toString(), QString("june@example.test"));
      QVERIFY(window.click("titleCancel"));
      QCOMPARE(document_elements(*window.flow.document()).back().id, initial.elements.back().id);
    }
  }
  void imported_title_form_keeps_metadata_and_multiline_source_with_export_gates() {
    const auto input = path("title-import") + ".fountain";
    const auto original = bytes(APOLLO_IMPORT_FIXTURE_DIR "/writer-subset.fountain");
    replace_file(input, original);
    const auto state = path("title-import-state");
    QString directory;
    QByteArray metadata;
    {
      Window window(font_, state, {}, {}, input);
      QTRY_VERIFY(window.flow.intake_preview());
      QTRY_VERIFY(!window.flow.busy());
      window.item("intakeProjectName")->setProperty("text", "Title import " + QString::number(count_));
      window.item("intakeProjectParent")->setProperty("text", root_.path());
      QVERIFY(window.click("editIntake"));
      QTRY_VERIFY(window.flow.document());
      directory = window.flow.autosaver()->location();
      metadata = bytes(directory + "/imported-material.json");
      const auto before = document_elements(*window.flow.document());
      QVERIFY(window.click("titlePageButton"));
      auto* contact = window.item("titleField_contact_4");
      QVERIFY(contact);
      contact->forceActiveFocus();
      QMetaObject::invokeMethod(contact, "selectAll");
      window.type("June Vale");
      QTest::keyClick(&window.view, Qt::Key_Return);
      QTest::keyClick(&window.view, Qt::Key_Return);
      window.type("june@example.test");
      QVERIFY(window.click("titleAdd_author"));
      QTRY_VERIFY(window.item("titleField_author_6"));
      window.type("René Wells");
      QVERIFY(window.click("titleApply"));
      const auto elements = document_elements(*window.flow.document());
      QCOMPARE(elements[4].text, std::string("June Vale\n\njune@example.test"));
      const auto author = std::find_if(elements.begin(), elements.end(), [](const auto& e) { return e.text == "René Wells"; });
      QVERIFY(author != elements.end());
      QCOMPARE(author->type, ElementType::author);
      QCOMPARE(elements[0].id, before[0].id);
      QCOMPARE(elements.back(), before.back());
      auto* layout = static_cast<ScreenplayLayout*>(window.flow.document()->documentLayout());
      QVERIFY2(layout->error().isEmpty(), qPrintable(layout->error()));
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      QVERIFY(!job->check(true).isEmpty());
      QVERIFY(job->check(false).isEmpty());
      const auto pdf = path("import-title-pdf") + ".pdf";
      replace_file(pdf, "Existing PDF");
      auto* pdf_job = qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>());
      QVERIFY(!pdf_job->request(pdf, false));
      QCOMPARE(bytes(pdf), QByteArray("Existing PDF"));
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(document_elements(*window.flow.document()), before);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      QCOMPARE(document_elements(*window.flow.document()), elements);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(bytes(directory + "/imported-material.json"), metadata);
      QCOMPARE(bytes(directory + "/imports/original.fountain"), original);
    }
    {
      Window window(font_, state, directory);
      QTRY_VERIFY(window.flow.document());
      QVERIFY(window.click("titlePageButton"));
      QTRY_VERIFY(window.popup_open("titlePagePanel"));
      QCOMPARE(window.item("titleField_contact_4")->property("text").toString(), QString("June Vale\n\njune@example.test"));
      QVERIFY(window.click("titleCancel"));
      QVERIFY(window.click("importedMaterial"));
      QTRY_VERIFY(window.item("intakeSource"));
      QCOMPARE(window.item("intakeSource")->property("text").toString().toUtf8(), original);
      QCOMPARE(bytes(directory + "/imported-material.json"), metadata);
    }
    QCOMPARE(bytes(input), original);
  }
  void emphasis_authoring_survives_save_reopen_and_export() {
    const auto fixture = QStringLiteral(APOLLO_IMPORT_FIXTURE_DIR "/emphasis-authoring");
    const auto original = bytes(fixture + ".fountain");
    const auto initial = parse_screenplay(bytes(fixture + ".expected.json"));
    const auto authored = parse_screenplay(bytes(fixture + ".authored.expected.json"));
    const auto exported = bytes(APOLLO_IMPORT_FIXTURE_DIR "/../export/emphasis-authoring.fountain");
    QVERIFY(!original.isEmpty() && !exported.isEmpty());
    const auto requested_output = qEnvironmentVariable("APOLLO_EMPHASIS_OUTPUT");
    const auto output_root = requested_output.isEmpty() ? path("emphasis-output") : requested_output;
    QVERIFY(QDir().mkpath(output_root));
    auto writing = [](const QTextDocument* document) { return canonical(Screenplay{0, document_elements(*document)}); };
    auto saved = [](const QString& directory) {
      auto screenplay = open_project(directory).screenplay;
      screenplay.revision = 0;
      return screenplay;
    };
    // The same real authoring actions protect ordinary writing and the imported
    // editable-copy path, without manufacturing styled project JSON.
    for (const bool imported : {false, true}) {
      const auto state = path("emphasis-state");
      const auto input = path("emphasis-input") + ".fountain";
      const auto output = imported ? path("emphasis-imported") + ".fountain" : output_root + "/authored.fountain";
      replace_file(input, original);
      QString working = imported ? QString() : create_project(output_root + "/project", initial).directory;
      {
        Window window(font_, state, working, {}, imported ? input : QString());
        if (imported) {
          QTRY_VERIFY(window.flow.intake_preview());
          QTRY_VERIFY(!window.flow.busy());
          QVERIFY(window.flow.intake_edit_supported());
          window.item("intakeProjectName")->setProperty("text", "Emphasis copy " + QString::number(count_));
          window.item("intakeProjectParent")->setProperty("text", root_.path());
          QVERIFY(window.click("editIntake"));
        }
        QTRY_VERIFY(window.flow.document());
        QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
        working = window.flow.autosaver()->location();
        QCOMPARE(writing(window.flow.document()), initial);
        const auto unchanged_text = window.flow.document()->toPlainText();
        auto expected = initial;
        for (int step = 0; step < 3; ++step) {
          const auto before = expected;
          const auto block = window.flow.document()->findBlockByNumber(step == 2 ? 3 : 1);
          const int start = block.position() + (step == 0 ? 0 : step == 1 ? 5 : 9);
          const int end = start + (step == 0 ? 4 : step == 1 ? 5 : 3);
          QVERIFY(QMetaObject::invokeMethod(window.flow.editor(), "select", Q_ARG(int, start), Q_ARG(int, end)));
          if (step < 2) {
            QTest::keyClick(&window.view, step == 0 ? Qt::Key_B : Qt::Key_I, Qt::ControlModifier);
          } else {
            QVERIFY(window.click("emphasisButton"));
            QTRY_VERIFY(window.popup_open("emphasisMenu"));
            QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/emphasis-menu.png")));
            QTRY_VERIFY(window.item("emphasisUnderline"));
            QVERIFY(window.click("emphasisUnderline"));
          }
          // Code-point offsets are independently specified by the fixtures;
          // the real document stores Café's bold range as five UTF-8 bytes.
          expected.elements[step == 2 ? 3 : 1].styles =
              authored.elements[step == 2 ? 3 : 1].styles;
          if (step == 0) expected.elements[1].styles.resize(1);
          QCOMPARE(writing(window.flow.document()), expected);
          QCOMPARE(window.flow.document()->toPlainText(), unchanged_text);
          QCOMPARE(window.flow.editor()->property("selectionStart").toInt(), start);
          QCOMPARE(window.flow.editor()->property("selectionEnd").toInt(), end);
          QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
          QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
          QCOMPARE(writing(window.flow.document()), before);
          QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
          QCOMPARE(writing(window.flow.document()), expected);
        }
        QCOMPARE(writing(window.flow.document()), authored);
        window.flow.editor()->setProperty("cursorPosition", 0);
        QTest::qWait(30);
        QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/emphasis-authored.png")));
        QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
        QCOMPARE(saved(working), authored);
      }
      {
        Window window(font_, state, working);
        QTRY_VERIFY(window.flow.document());
        QCOMPARE(writing(window.flow.document()), authored);
        auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
        QVERIFY(job);
        QTest::keyClick(&window.view, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(window.popup_open("exportPanel"));
        QVERIFY(window.item("exportEligibility")->property("text").toString().startsWith("Ready."));
        window.item("fountainDestination")->setProperty("text", output);
        QVERIFY(window.click("exportConfirm"));
        QTRY_VERIFY(!job->busy());
        QCOMPARE(bytes(output), exported);
        if (imported) {
          const auto pdf = path("emphasis-blocked") + ".pdf";
          replace_file(pdf, "Existing PDF");
          auto* pdf_job = qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>());
          QVERIFY(pdf_job);
          QVERIFY(!pdf_job->request(pdf));
          QCOMPARE(bytes(pdf), QByteArray("Existing PDF"));
          QVERIFY(pdf_job->status().contains("remain blocked"));
          QVERIFY(window.click("importedMaterial"));
          QTRY_VERIFY(window.item("intakeSource"));
          QCOMPARE(window.item("intakeSource")->property("text").toString().toUtf8(), original);
          QVERIFY(window.item("intakeSource")->property("readOnly").toBool());
          QCOMPARE(bytes(working + "/imports/original.fountain"), original);
        }
      }
      QCOMPARE(saved(working), authored);
      QCOMPARE(bytes(input), original);
    }
  }
  void manual_page_break_authoring_survives_save_reopen_and_export() {
    const auto initial = parse_screenplay(bytes(APOLLO_IMPORT_FIXTURE_DIR "/../editing/page-break-authoring.screenplay.json"));
    const auto expected_fountain = bytes(APOLLO_IMPORT_FIXTURE_DIR "/../export/page-break-authoring.fountain");
    QVERIFY(!expected_fountain.isEmpty());
    const auto requested = qEnvironmentVariable("APOLLO_PAGE_BREAK_OUTPUT");
    const auto output_root = requested.isEmpty() ? path("page-break-output") : requested;
    QVERIFY(QDir().mkpath(output_root));
    const auto project = create_project(output_root + "/project", initial);
    const auto state = path("page-break-state");
    auto writing = [](const QTextDocument* document) {
      return canonical(Screenplay{0, document_elements(*document)});
    };
    auto without_markers = [](Screenplay screenplay) {
      std::erase_if(screenplay.elements, [](const Element& element) { return element.type == ElementType::page_break; });
      return screenplay;
    };
    Screenplay authored;
    {
      Window window(font_, state, project.directory);
      QTRY_VERIFY(window.flow.editor() && window.flow.editor()->hasActiveFocus());
      auto* document = window.flow.document();
      auto* editor = window.flow.editor();
      auto* commands = document->findChild<EditorCommands*>();
      QVERIFY(commands);
      auto block = [&](const char* id) {
        for (auto current = document->begin(); current.isValid(); current = current.next())
          if (block_element(current).id == id) return current;
        return QTextBlock();
      };
      auto caret = [&](const char* id, int offset = 0) {
        editor->setProperty("cursorPosition", block(id).position() + offset);
        editor->forceActiveFocus();
      };
      // Selected insertion is refused even in ordinary writing, without
      // collapsing the selection or changing emphasis or existing identities.
      const auto start = block("opening").position() + 2;
      QVERIFY(QMetaObject::invokeMethod(editor, "select", Q_ARG(int, start), Q_ARG(int, start + 5)));
      QTest::keyClick(&window.view, Qt::Key_Return, Qt::ControlModifier);
      QCOMPARE(writing(document), initial);
      QCOMPARE(editor->property("selectionStart").toInt(), start);
      QCOMPARE(editor->property("selectionEnd").toInt(), start + 5);
      QVERIFY(!commands->property("pageBreakMessage").toString().isEmpty());
      caret("single-dialogue", 5);
      QVERIFY(window.click("pageBreakButton"));
      QTRY_VERIFY(window.popup_open("pageBreakMenu"));
      QVERIFY(window.click("pageBreakInsert"));
      QTRY_VERIFY(editor->hasActiveFocus());
      const auto first = writing(document);
      QCOMPARE(without_markers(first), initial);
      QCOMPARE(first.elements.size(), initial.elements.size() + 1);
      QCOMPARE(first.elements[3].type, ElementType::page_break); // title, heading, action, marker, cue...
      QVERIFY(first.elements[3].text.empty());
      const auto first_marker = first.elements[3].id;
      QVERIFY(!first_marker.empty());
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(document), initial);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      QCOMPARE(writing(document), first);
      // A right-hand dialogue caret inserts before the complete dual pair,
      // never between cue/dialogue or between its left and right speeches.
      caret("right-dialogue", 4);
      QTest::keyClick(&window.view, Qt::Key_Return, Qt::ControlModifier);
      authored = writing(document);
      QCOMPARE(without_markers(authored), initial);
      QCOMPARE(authored.elements.size(), initial.elements.size() + 2);
      QCOMPARE(authored.elements[8].type, ElementType::page_break);
      QVERIFY(authored.elements[8].text.empty());
      const auto second_marker = authored.elements[8].id;
      QVERIFY(!second_marker.empty() && second_marker != first_marker);
      QCOMPARE(authored.elements[9].id, std::string("left-cue"));
      QCOMPARE(authored.elements[11].id, std::string("right-cue"));
      QVERIFY(authored.elements[11].dual_right);
      // Removing through the discoverable control touches only the marker.
      caret(second_marker.c_str());
      QVERIFY(window.click("pageBreakButton"));
      QTRY_VERIFY(window.popup_open("pageBreakMenu"));
      QVERIFY(window.click("pageBreakRemove"));
      QCOMPARE(writing(document), first);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(document), authored);
      caret(first_marker.c_str());
      QTest::keyClick(&window.view, Qt::Key_Return, Qt::ControlModifier | Qt::ShiftModifier);
      auto only_second = authored;
      only_second.elements.erase(only_second.elements.begin() + 3);
      QCOMPARE(writing(document), only_second);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(document), authored);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      QCOMPARE(writing(document), only_second);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(document), authored);
      // Boundary deletion removes exactly one marker, not either text block
      // or the paragraph separator joining them. Undo preserves marker ID.
      caret("single-cue");
      QTest::keyClick(&window.view, Qt::Key_Backspace);
      QCOMPARE(writing(document), only_second);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(document), authored);
      caret("bridge", block("bridge").text().size());
      QTest::keyClick(&window.view, Qt::Key_Delete);
      QCOMPARE(writing(document), first);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(document), authored);
      // Cross-marker replacement, Enter and deletion all preserve selection.
      const auto from = block("opening").position() + 2;
      const auto to = block("single-cue").position() + 2;
      QVERIFY(QMetaObject::invokeMethod(editor, "select", Q_ARG(int, from), Q_ARG(int, to)));
      window.type("X");
      QCOMPARE(writing(document), authored);
      for (const auto key : {Qt::Key_Return, Qt::Key_Backspace, Qt::Key_Delete}) {
        QTest::keyClick(&window.view, key);
        QCOMPARE(writing(document), authored);
        QCOMPARE(editor->property("selectionStart").toInt(), from);
        QCOMPARE(editor->property("selectionEnd").toInt(), to);
      }
      // Typing while on the marker redirects to editable writing; the marker
      // stays an empty non-text element and one undo restores all source.
      caret(first_marker.c_str());
      window.type("X");
      const auto typed = writing(document);
      QCOMPARE(typed.elements[3], authored.elements[3]);
      QCOMPARE(typed.elements[4].text, std::string("XMARA"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(document), authored);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      auto saved = open_project(project.directory).screenplay;
      saved.revision = 0;
      QCOMPARE(saved, authored);
    }
    {
      Window window(font_, state, project.directory);
      QTRY_VERIFY(window.flow.editor() && window.flow.editor()->hasActiveFocus());
      QCOMPARE(writing(window.flow.document()), authored);
      auto* fountain_job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      auto* pdf_job = qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>());
      QVERIFY(fountain_job && pdf_job);
      QTest::keyClick(&window.view, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(window.item("exportEligibility")->property("text").toString().startsWith("Ready."));
      window.item("fountainDestination")->setProperty("text", output_root + "/authored.fountain");
      QVERIFY(window.click("exportConfirm"));
      QTRY_VERIFY(!fountain_job->busy());
      QCOMPARE(bytes(output_root + "/authored.fountain"), expected_fountain);
      QVERIFY(window.click("exportButton"));
      QVERIFY(window.click("exportFormatPdf"));
      QVERIFY(window.item("includeTitle")->property("checked").toBool());
      window.item("exportDestination")->setProperty("text", output_root + "/authored.pdf");
      QSignalSpy completed(pdf_job, &PdfExportJob::completed);
      QVERIFY(window.click("exportConfirm"));
      QTRY_VERIFY(!pdf_job->busy());
      QCOMPARE(completed.size(), 1);
      QVERIFY(completed[0][0].toBool());
      QVERIFY(bytes(output_root + "/authored.pdf").startsWith("%PDF-"));
      QCOMPARE(writing(window.flow.document()), authored);
      // A legacy source value is simulated directly, not through authoring.
      // Its exact text remains saveable; the real PDF helper must refuse it
      // without replacing the already successful PDF or changing the source.
      const auto previous_pdf = bytes(output_root + "/authored.pdf");
      QTextCursor legacy(window.flow.document()->findBlockByNumber(3));
      legacy.insertText(QStringLiteral("Legacy words."));
      auto retained = authored;
      retained.elements[3].text = "Legacy words.";
      QCOMPARE(writing(window.flow.document()), retained);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      const auto legacy_source = bytes(project.directory + "/screenplay.json");
      completed.clear();
      QVERIFY(pdf_job->request(output_root + "/authored.pdf"));
      QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 15000);
      QVERIFY(!completed[0][0].toBool());
      QCOMPARE(bytes(output_root + "/authored.pdf"), previous_pdf);
      QCOMPARE(bytes(project.directory + "/screenplay.json"), legacy_source);
      QCOMPARE(writing(window.flow.document()), retained);
      window.flow.document()->undo();
      QCOMPARE(writing(window.flow.document()), authored);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
    }
    auto saved = open_project(project.directory).screenplay;
    saved.revision = 0;
    QCOMPARE(saved, authored);
  }
  void scene_spacing_choice_and_empty_speech_resolution_survive_reopen_and_export() {
    const auto input = path("scene-spacing") + ".fountain";
    const auto original = bytes(APOLLO_IMPORT_FIXTURE_DIR "/scene-spacing.fountain");
    replace_file(input, original);
    auto expected = parse_screenplay(bytes(APOLLO_IMPORT_FIXTURE_DIR "/scene-spacing.expected.json"));
    const auto state = path("scene-spacing-state");
    const auto output = path("scene-spacing-out") + ".fountain";
    auto exported = bytes(APOLLO_IMPORT_FIXTURE_DIR "/../export/scene-spacing.fountain");
    exported.replace("The dial stops.\n", "The dial stops.t\n");
    QString working;
    {
      Window window(font_, state, {}, {}, input);
      QTRY_VERIFY(window.flow.intake_preview()); QTRY_VERIFY(!window.flow.busy());
      QCOMPARE(window.flow.intake_scene_spacing_count(), 2);
      QVERIFY(!window.flow.intake_edit_supported());
      QVERIFY(window.click("normalizeSceneSpacing"));
      QVERIFY(window.flow.intake_edit_supported());
      QVERIFY(window.click("normalizeSceneSpacing"));
      QVERIFY(!window.flow.intake_edit_supported());
      QVERIFY(window.click("normalizeSceneSpacing"));
      window.item("intakeProjectName")->setProperty("text", "Scene spacing copy");
      window.item("intakeProjectParent")->setProperty("text", root_.path());
      QVERIFY(window.click("editIntake")); QTRY_VERIFY(window.flow.document());
      working = window.flow.autosaver()->location();
      auto* commands = window.flow.document()->findChild<EditorCommands*>();
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      QVERIFY(commands && job);
      QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), expected);
      QCOMPARE(commands->emptySpeeches().size(), 2);
      replace_file(output, "Existing Fountain");
      QVERIFY(!job->request(output));
      QCOMPARE(bytes(output), QByteArray("Existing Fountain"));
      QTest::qWait(30);
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/empty-speech-review.png")));
      // Select and explicitly correct each issue through the actual controls.
      for (int correction = 0; correction < 2; ++correction) {
        QVERIFY(window.click("reviewEmptySpeeches"));
        QTRY_VERIFY(find_item(window.view.contentItem(), "emptySpeechIssue0"));
        QTest::qWait(30);
        auto* issue = find_item(window.view.contentItem(), "emptySpeechIssue0");
        QTest::mouseClick(&window.view, Qt::LeftButton, Qt::NoModifier,
                         issue->mapToScene(issue->boundingRect().center()).toPoint());
        QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
        QVERIFY(window.click("treatSpeechAsAction"));
        const auto before = expected;
        expected.elements[3 + correction * 2].type = expected.elements[4 + correction * 2].type = ElementType::action;
        QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), expected);
        QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), before);
        QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), expected);
      }
      QVERIFY(commands->emptySpeeches().isEmpty());
      const auto last = window.flow.document()->lastBlock();
      window.flow.editor()->setProperty("cursorPosition", last.position() + last.length() - 1);
      window.type("t");
      expected.elements.back().text += "t";
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QTest::keyClick(&window.view, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(window.item("exportEligibility")->property("text").toString().contains("2 extra empty lines before scene headings"));
      window.item("fountainDestination")->setProperty("text", output);
      QVERIFY(window.click("exportConfirm")); QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(output), exported);
      QVERIFY(window.click("importedMaterial"));
      QTRY_VERIFY(window.item("intakeSource"));
      QCOMPARE(window.item("intakeSource")->property("text").toString().toUtf8(), original);
      QVERIFY(window.item("intakeSummary")->property("text").toString().contains("not laid out or written"));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/scene-spacing-retained.png")));
      QVERIFY(window.click("intakeHome"));
      QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
    }
    {
      Window window(font_, state, working);
      QTRY_VERIFY(window.flow.document());
      QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), expected);
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      QTest::keyClick(&window.view, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
      QTRY_VERIFY(window.popup_open("exportPanel"));
      window.item("fountainDestination")->setProperty("text", output);
      QVERIFY(window.click("exportConfirm")); QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(output), exported);
      const auto pdf = path("scene-spacing-blocked") + ".pdf";
      replace_file(pdf, "Existing PDF");
      QVERIFY(!qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>())->request(pdf));
      QCOMPARE(bytes(pdf), QByteArray("Existing PDF"));
    }
    QCOMPARE(bytes(input), original);
    QCOMPARE(bytes(working + "/imports/original.fountain"), original);
  }
  void editable_import_preserves_content_and_associations_in_actual_writer_data() {
    QTest::addColumn<bool>("crlf");
    QTest::newRow("LF") << false;
    QTest::newRow("BOM-CRLF") << true;
  }
  void editable_import_preserves_content_and_associations_in_actual_writer() {
    QFETCH(bool, crlf);
    const auto fixture = QStringLiteral(APOLLO_IMPORT_FIXTURE_DIR "/writer-subset");
    auto original = bytes(fixture + ".fountain");
    if (crlf) { original.replace("\n", "\r\n"); original.prepend("\xef\xbb\xbf"); }
    auto expected = parse_screenplay(bytes(fixture + ".expected.json"));
    const auto input = path("writer-input"); replace_file(input, original);
    const auto review_assets = interpreted_fountain_assets(read_fountain_intake(input).assets);
    const auto review = create_project(path("writer-review"), {0, {{ElementType::action, ""}}}, review_assets);
    const auto review_manifest = bytes(review.directory + "/project.json");
    const auto review_source = bytes(review.screenplay_path());
    const auto state = path("writer-state");
    QString working, copied;
    ProjectAssets preserved_assets;
    auto writing = [](const QTextDocument* doc) { return canonical(Screenplay{0, document_elements(*doc)}); };
    auto saved_writing = [](const QString& path) { auto s = open_project(path).screenplay; s.revision = 0; return s; };
    auto association = [](const Window& window, const QString& anchor) {
      for (const auto& item : window.flow.intake_findings()) {
        const auto f = item.toMap();
        if (f.value("anchor") == anchor) return f.value("association").toString();
      }
      return QString();
    };
    {
      Window window(font_, state, review.directory);
      QTRY_VERIFY(window.flow.intake_interpreted()); QTRY_VERIFY(!window.flow.busy());
      QVERIFY(!window.flow.document());
      QVERIFY(window.flow.intake_edit_supported());
      window.item("intakeProjectName")->setProperty("text", "Writer " + QString::number(count_));
      window.item("intakeProjectParent")->setProperty("text", root_.filePath("missing-parent"));
      QVERIFY(window.click("editIntake")); QTRY_VERIFY(!window.flow.busy());
      QVERIFY(window.flow.intake_active() && !window.flow.document());
      QVERIFY_THROWS_EXCEPTION(SourceError, ProjectSession(review.directory));
      window.item("intakeProjectParent")->setProperty("text", root_.path());
      QVERIFY(window.click("editIntake")); QTRY_VERIFY(window.flow.document());
      QVERIFY(window.flow.intake_working());
      QVERIFY(qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>())->status().contains("remain blocked"));
      working = window.flow.autosaver()->location();
      const auto p = open_project(working);
      preserved_assets = p.assets;
      QVERIFY(p.project.id != review.id && p.project.import_working);
      QCOMPARE(writing(window.flow.document()), expected);
      QCOMPARE(saved_writing(working), expected);
      auto* layout = static_cast<ScreenplayLayout*>(window.flow.document()->documentLayout());
      QVERIFY2(layout->error().isEmpty(), qPrintable(layout->error()));
      // Hand-derived profile slots/columns, independent of the layout engine.
      const auto shown = shown_lines(*window.flow.document());
      for (const auto& want : std::vector<ShownLine>{{1, 3, "action", 108, QString::fromUtf8("Café doors open.")},
          {1, 4, "action", 108, "A clock ticks."}, {1, 8, "dual-dialogue-L", 108, "First line."},
          {1, 9, "dual-dialogue-L", 108, "Second line."}}) {
        QVERIFY2(std::any_of(shown.begin(), shown.end(), [&](const auto& line) {
          return line.page == want.page && line.slot == want.slot && line.kind == want.kind && line.text == want.text && std::abs(line.x - want.x) < .01;
        }), qPrintable(want.text));
      }
      QCOMPARE(association(window, "f8"), QString("Present: f8"));
      QVERIFY(window.click("importedMaterial"));
      QTRY_VERIFY(window.item("intakeSource"));
      QCOMPARE(window.flow.intake_source().toUtf8(), crlf ? original.mid(3) : original);
      QCOMPARE(window.item("intakeSource")->property("text").toString().toUtf8(), bytes(fixture + ".fountain"));
      QVERIFY(window.item("intakeSource")->property("readOnly").toBool());
      QVERIFY(window.click("intakeHome"));
      QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
      // Split action: the original association stays on the first part, undo
      // restores the exact independent screenplay and identities.
      auto action = window.flow.document()->findBlockByNumber(7);
      QCOMPARE(block_element(action).id, std::string("f8"));
      window.flow.editor()->setProperty("cursorPosition", action.position() + 5);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QCOMPARE(block_element(window.flow.document()->findBlockByNumber(7)).id, std::string("f8"));
      QVERIFY(block_element(window.flow.document()->findBlockByNumber(8)).id != "f8");
      QCOMPARE(association(window, "f8"), QString("Present: f8"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(window.flow.document()), expected);
      // Joining with the preceding heading keeps that heading's identity;
      // the removed action anchor is explicit, and undo reconnects it.
      action = window.flow.document()->findBlockByNumber(7);
      window.flow.editor()->setProperty("cursorPosition", action.position());
      QTest::keyClick(&window.view, Qt::Key_Backspace);
      QVERIFY(association(window, "f8").startsWith("Detached:"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(window.flow.document()), expected);
      QCOMPARE(association(window, "f8"), QString("Present: f8"));
      // A whole-element deletion detaches the note instead of retargeting it.
      action = window.flow.document()->findBlockByNumber(7);
      QMetaObject::invokeMethod(window.flow.editor(), "select", Q_ARG(int, action.position()), Q_ARG(int, action.next().position()));
      QTest::keyClick(&window.view, Qt::Key_Delete);
      QVERIFY(association(window, "f8").startsWith("Detached:"));
      QVERIFY(window.click("importedMaterial"));
      QTRY_VERIFY(window.item("intakeFindingText"));
      QVERIFY(window.item("intakeFindingText")->property("text").toString().contains("Detached: f8"));
      QVERIFY(window.item("intakeFindingText")->property("text").toString().contains("Remember the blue door."));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/import-working-reader.png")));
      QVERIFY(window.click("intakeHome"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(window.flow.document()), expected);
      QCOMPARE(association(window, "f8"), QString("Present: f8"));
      // A type change is visible; undo restores the association's type.
      action = window.flow.document()->findBlockByNumber(7);
      window.flow.editor()->setProperty("cursorPosition", action.position());
      QTest::keyClick(&window.view, Qt::Key_Tab);
      QCOMPARE(association(window, "f8"), QString("Type changed: f8"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(window.flow.document()), expected);
      // Real Enter within dialogue: a deliberate break in one element, with
      // independently specified text and style offsets after redo.
      const auto dialogue = window.flow.document()->findBlockByNumber(10);
      QCOMPARE(block_element(dialogue).id, std::string("f11"));
      window.flow.editor()->setProperty("cursorPosition", dialogue.position() + 2);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(window.flow.document()), expected);
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      expected.elements[10].text = "Fi\nrst line.\nSecond line.";
      expected.elements[10].styles = {{7, 11, Style::italic}, {20, 24, Style::underline}};
      QCOMPARE(writing(window.flow.document()), expected);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(saved_writing(working), expected);
      QVERIFY(layout->error().isEmpty());
      // Scene reordering preserves element IDs and their historical records.
      const auto heading = window.flow.document()->findBlockByNumber(15);
      window.flow.editor()->setProperty("cursorPosition", heading.position());
      QTest::keyClick(&window.view, Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier);
      QCOMPARE(association(window, "f7"), QString("Present: f7"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(writing(window.flow.document()), expected);
      // Persist a removed anchor as well: its stashed passage must remain
      // available through reopen, recovery and future copies.
      const auto transition = window.flow.document()->findBlockByNumber(14);
      QCOMPARE(block_element(transition).id, std::string("f15"));
      QMetaObject::invokeMethod(window.flow.editor(), "select", Q_ARG(int, transition.position()), Q_ARG(int, transition.next().position()));
      QTest::keyClick(&window.view, Qt::Key_Delete);
      expected.elements.erase(expected.elements.begin() + 14);
      QVERIFY(association(window, "f15").startsWith("Detached:"));
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(saved_writing(working), expected);
      const auto blocked_pdf = path("blocked.pdf"); replace_file(blocked_pdf, "Existing PDF evidence");
      QVERIFY(!qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>())->request(blocked_pdf));
      QCOMPARE(bytes(blocked_pdf), QByteArray("Existing PDF evidence"));
      QVERIFY(qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>())->status().contains("remain blocked"));
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/import-writing.png")));
    }
    {
      Window window(font_, state);
      QTRY_VERIFY(window.flow.document());
      QCOMPARE(writing(window.flow.document()), expected);
      QCOMPARE(association(window, "f8"), QString("Present: f8"));
      QVERIFY(association(window, "f15").startsWith("Detached:"));
      QCOMPARE(open_project(working).assets, preserved_assets);
      copied = path("writer-copy");
      QVERIFY(window.click("saveCopy"));
      window.item("copyDestination")->setProperty("text", copied);
      QVERIFY(window.click("copyConfirm"));
      QTRY_COMPARE(window.flow.autosaver()->location(), copied);
      QCOMPARE(window.flow.location(), copied);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(saved_writing(copied), expected);
      QCOMPARE(open_project(copied).assets, preserved_assets);
      QCOMPARE(writing(window.flow.document()), expected);
      QCOMPARE(association(window, "f8"), QString("Present: f8"));
      QVERIFY(association(window, "f15").startsWith("Detached:"));
      QVERIFY(open_project(copied).project.id != open_project(working).project.id);
      // Continue editing in the adopted copy; only that copy changes.
      const auto last = window.flow.document()->lastBlock();
      window.flow.editor()->setProperty("cursorPosition", last.position() + last.length() - 1);
      window.type(" Still moving.");
      expected.elements.back().text = "The train leaves. Still moving.";
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(saved_writing(copied), expected);
      QVERIFY(saved_writing(working).elements.back().text == "The train leaves.");
    }
    const auto complete = open_project(copied);
    const auto damaged = QByteArray("{damaged imported screenplay\n");
    replace_file(complete.project.screenplay_path(), damaged);
    {
      Window window(font_, path("recover-import-state"), copied);
      QTRY_VERIFY(!window.flow.busy());
      QVERIFY(!window.flow.document() && !window.flow.current_available());
      int latest = -1;
      for (int i = 0; i < window.flow.candidates().size(); ++i)
        if (window.flow.candidates()[i].toMap().value("revision").toULongLong() == complete.screenplay.revision) latest = i;
      QVERIFY(latest >= 0);
      const auto package = window.flow.candidates()[latest].toMap().value("location").toString();
      const auto package_source = bytes(package + "/screenplay.json");
      window.item("recoveryDestination")->setProperty("text", path("recovered-import"));
      const auto choice = QString("recoveryChoice%1").arg(latest).toLatin1();
      QVERIFY(window.click(choice.constData())); QVERIFY(window.click("recoverProject"));
      QTRY_VERIFY(window.flow.document());
      QCOMPARE(writing(window.flow.document()), expected);
      QCOMPARE(association(window, "f8"), QString("Present: f8"));
      QVERIFY(association(window, "f15").startsWith("Detached:"));
      QCOMPARE(open_project(window.flow.autosaver()->location()).assets, preserved_assets);
      QVERIFY(!qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>())->request(path("blocked-recovery.pdf")));
      QCOMPARE(bytes(complete.project.screenplay_path()), damaged);
      QCOMPARE(bytes(package + "/screenplay.json"), package_source);
      const auto last = window.flow.document()->lastBlock();
      window.flow.editor()->setProperty("cursorPosition", last.position() + last.length() - 1);
      window.type(" Recovered.");
      expected.elements.back().text = "The train leaves. Still moving. Recovered.";
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(saved_writing(window.flow.autosaver()->location()), expected);
    }
    QCOMPARE(bytes(review.directory + "/project.json"), review_manifest);
    QCOMPARE(bytes(review.screenplay_path()), review_source);
    QCOMPARE(open_project(review.directory).assets, review_assets);
    QCOMPARE(bytes(input), original);
    QCOMPARE(*preserved_assets.original_fountain, original);
  }
  void punctuation_import_edit_reopen_and_export_preserves_authored_unicode() {
    const auto fixture = QStringLiteral(APOLLO_IMPORT_FIXTURE_DIR "/punctuation-subset");
    const auto original = bytes(fixture + ".fountain");
    const auto input = path("punctuation-input");
    replace_file(input, original);
    auto expected = parse_screenplay(bytes(fixture + ".expected.json"));
    auto exported = bytes(APOLLO_IMPORT_FIXTURE_DIR "/../export/punctuation-subset.fountain");
    const auto state = path("punctuation-state"), output = path("punctuation-output") + ".fountain";
    QString working;
    {
      Window window(font_, state, {}, {}, input);
      QTRY_VERIFY(window.flow.intake_preview());
      QVERIFY(window.flow.intake_edit_supported());
      window.item("intakeProjectName")->setProperty("text", "Punctuation " + QString::number(count_));
      window.item("intakeProjectParent")->setProperty("text", root_.path());
      QVERIFY(window.click("editIntake"));
      QTRY_VERIFY(window.flow.document());
      working = window.flow.autosaver()->location();
      QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), expected);
      QVERIFY(draws(font_, window.flow.document()->findBlockByNumber(1), QString::fromUtf8(expected.elements[1].text)));
      // Every newly admitted code point has a real fixed-pitch glyph in each
      // pinned face, rather than silently relying on font fallback.
      for (const auto weight : {QFont::Normal, QFont::Bold}) for (const bool italic : {false, true}) {
        auto font = font_; font.setWeight(weight); font.setItalic(italic);
        const auto raw = QRawFont::fromFont(font);
        const auto glyphs = raw.glyphIndexesForString(QStringLiteral("—‘’“”…"));
        for (const auto glyph : glyphs) QVERIFY(glyph != 0);
        for (const auto advance : raw.advancesForGlyphIndexes(glyphs))
          QVERIFY(std::abs(advance.x() / px_per_point - profile::advance) < 0.01);
      }
      auto* editor = window.flow.editor();
      const auto last = window.flow.document()->lastBlock();
      editor->setProperty("cursorPosition", last.position() + last.length() - 1);
      editor->forceActiveFocus();
      window.type(QStringLiteral("…"));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(block_element(last).text, std::string("The lamp turns."));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      expected.elements.back().text += "…";
      exported.replace("The lamp turns.\n", "The lamp turns.…\n");
      QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), expected);
      QVERIFY(static_cast<ScreenplayLayout*>(window.flow.document()->documentLayout())->error().isEmpty());
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/punctuation-writing.png")));
    }
    {
      Window window(font_, state);
      QTRY_VERIFY(window.flow.document());
      QCOMPARE(canonical(Screenplay{0, document_elements(*window.flow.document())}), expected);
      QTest::keyClick(&window.view, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(window.item("exportEligibility")->property("text").toString().startsWith("Ready."));
      window.item("fountainDestination")->setProperty("text", output);
      QVERIFY(window.click("exportConfirm"));
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(output), exported);
      const auto pdf = path("blocked-punctuation") + ".pdf";
      replace_file(pdf, "Existing PDF evidence");
      QVERIFY(!qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>())->request(pdf));
      QCOMPARE(bytes(pdf), QByteArray("Existing PDF evidence"));
    }
    QCOMPARE(bytes(input), original);
    QCOMPARE(bytes(working + "/imports/original.fountain"), original);
  }

  // Import → real edits → autosave → reopen → clean Fountain, compared with
  // hand-written expected text. Eligibility follows each snapshot, refusals
  // and failures keep existing files, and writing continues during export.
  void editable_import_exports_clean_fountain_snapshots_through_real_controls() {
    const auto input = path("fountain-input");
    replace_file(input, bytes(APOLLO_IMPORT_FIXTURE_DIR "/writer-subset.fountain"));
    const auto review = create_project(path("fountain-review"), {0, {{ElementType::action, ""}}},
                                       interpreted_fountain_assets(read_fountain_intake(input).assets));
    const auto state = path("fountain-state");
    const auto exported = path("exported") + ".fountain";
    const auto pdf = path("blocked") + ".pdf";
    replace_file(pdf, "Existing PDF evidence");
    auto expected = [](const char* name) { return bytes(QStringLiteral(APOLLO_IMPORT_FIXTURE_DIR "/../export/") + name); };
    QString working;
    {
      Window window(font_, state, review.directory);
      QTRY_VERIFY(window.flow.intake_interpreted()); QTRY_VERIFY(!window.flow.busy());
      window.item("intakeProjectName")->setProperty("text", "Exporting " + QString::number(count_));
      window.item("intakeProjectParent")->setProperty("text", root_.path());
      QVERIFY(window.click("editIntake")); QTRY_VERIFY(window.flow.document());
      working = window.flow.autosaver()->location();
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      QVERIFY(job);
      auto eligibility = [&] { return window.item("exportEligibility")->property("text").toString(); };
      auto close_panel = [&] { QTest::keyClick(&window.view, Qt::Key_Escape); QTRY_VERIFY(window.flow.editor()->hasActiveFocus()); };
      // The panel discloses the PDF block and, for Fountain, what is omitted.
      QVERIFY(window.click("exportButton"));
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(eligibility().contains("PDF exports remain blocked"));
      QVERIFY(window.click("exportFormatFountain"));
      QVERIFY(eligibility().startsWith("Ready."));
      QVERIFY(eligibility().contains("Not included in clean Fountain: 1 imported note, 1 stash passage"));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/fountain-export-ready.png")));
      window.item("fountainDestination")->setProperty("text", exported);
      QVERIFY(window.click("exportConfirm"));
      QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(exported), expected("writer-subset.fountain"));
      QVERIFY(job->status().contains("exported to " + exported));
      QVERIFY(job->status().contains("2 synopses, 1 production scene number"));
      // A deliberate break typed before a space leaves it at the start of the
      // new line, where it is not shown; the export leaves it out and says
      // so (docs/spacing.md). Delete removes it.
      auto speech = window.flow.document()->findBlockByNumber(12);
      QCOMPARE(block_element(speech).text, std::string("At the same time."));
      window.flow.editor()->setProperty("cursorPosition", speech.position() + 6);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QVERIFY(static_cast<const ScreenplayLayout*>(window.flow.document()->documentLayout())->error().isEmpty());
      QVERIFY(job->check(true).isEmpty());
      QVERIFY2(job->presentationNote().contains("Written without the spaces at the start or end of 1 line"), qPrintable(job->presentationNote()));
      QTest::keyClick(&window.view, Qt::Key_Delete);
      QCOMPARE(block_element(speech).text, std::string("At the\nsame time."));
      QVERIFY(job->presentationNote().isEmpty());
      // A speech that begins with a blank line cannot be written in Fountain:
      // the explanation names the element, the button stays disabled and
      // nothing is written. Backspace corrects it.
      window.flow.editor()->setProperty("cursorPosition", speech.position());
      QTest::keyClick(&window.view, Qt::Key_Return);
      QCOMPARE(block_element(speech).text, std::string("\nAt the\nsame time."));
      QTest::keyClick(&window.view, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY2(eligibility().contains("Element 13, Dialogue “At the”: begins or ends with a blank line, which Fountain cannot keep in the speech."),
               qPrintable(eligibility()));
      QVERIFY(!window.click("exportConfirm"));
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/fountain-export-blocked.png")));
      close_panel();
      QVERIFY(!job->request(exported));
      QVERIFY(job->status().startsWith("Fountain export blocked: Element 13"));
      QCOMPARE(bytes(exported), expected("writer-subset.fountain"));
      QTest::keyClick(&window.view, Qt::Key_Backspace);
      QCOMPARE(block_element(speech).text, std::string("At the\nsame time."));
      QVERIFY(job->check(true).isEmpty());
      // New writing: an empty element is explained until corrected with
      // ordinary keys. A cue typed in mixed case is presented, and exported, in
      // capitals; the document keeps what was typed.
      const auto last = window.flow.document()->lastBlock();
      window.flow.editor()->setProperty("cursorPosition", last.position() + last.length() - 1);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QCOMPARE(job->check(true), QString("Element 22, Action (empty): is empty; type its text or delete it."));
      QTest::keyClick(&window.view, Qt::Key_Backtab, Qt::ShiftModifier);
      window.type("INT. DEPOT - DAY");
      QTest::keyClick(&window.view, Qt::Key_Return);
      window.type("Mara waits.");
      QTest::keyClick(&window.view, Qt::Key_Return);
      QTest::keyClick(&window.view, Qt::Key_Tab);
      window.type("Mara");
      QTest::keyClick(&window.view, Qt::Key_Return);
      window.type("Here.");
      QVERIFY(job->check(true).isEmpty());
      QCOMPARE(block_element(window.flow.document()->findBlockByNumber(23)).text, std::string("Mara"));
      QCOMPARE(job->presentationNote(), QString("Written in capitals, as Apollo shows and prints them: 1 character cue typed in lowercase. "
                                        "The project keeps the case as typed."));
      // The accepted snapshot is fixed: typing continues while the worker is
      // held, and the later words are saved but not exported.
      std::mutex mutex;
      std::condition_variable condition;
      bool released = false;
      std::atomic<bool> entered = false;
      job->set_worker_hook([&] {
        std::unique_lock lock(mutex);
        entered = true;
        condition.wait(lock, [&] { return released; });
      });
      const auto release = qScopeGuard([&] {
        { std::lock_guard lock(mutex); released = true; }
        condition.notify_all();
      });
      QVERIFY(window.click("exportButton"));
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(eligibility().startsWith("Ready."));
      QVERIFY(eligibility().contains("1 character cue typed in lowercase"));
      QVERIFY(window.click("exportConfirm"));
      QTRY_VERIFY(entered.load());
      QVERIFY(job->busy());
      QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
      const auto end = window.flow.document()->lastBlock();
      window.flow.editor()->setProperty("cursorPosition", end.position() + end.length() - 1);
      window.type(" Later.");
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(open_project(working).screenplay.elements.back().text, std::string("Here. Later."));
      QVERIFY(window.item("operationStatus")->property("text").toString().startsWith("Exporting Fountain snapshot from"));
      { std::lock_guard lock(mutex); released = true; }
      condition.notify_all();
      QTRY_VERIFY(!job->busy());
      job->set_worker_hook({});
      QCOMPARE(bytes(exported), expected("writer-subset-edited.fountain"));
      QVERIFY(window.item("operationStatus")->property("text").toString().startsWith("Fountain snapshot from"));
      for (int i = 0; i < 7; ++i) QTest::keyClick(&window.view, Qt::Key_Backspace);
      // PDF remains separately blocked for imported writing.
      QVERIFY(!qobject_cast<PdfExportJob*>(window.view.rootObject()->property("exportJob").value<QObject*>())->request(pdf));
      QCOMPARE(bytes(pdf), QByteArray("Existing PDF evidence"));
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
    }
    QFile::remove(exported);
    {
      Window window(font_, state);
      QTRY_VERIFY(window.flow.document());
      QCOMPARE(window.flow.autosaver()->location(), working);
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      // Reopened writing, title page left out through the real checkbox.
      QVERIFY(window.click("exportButton"));
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(window.click("exportFormatFountain"));
      QVERIFY(window.click("includeTitle"));
      window.item("fountainDestination")->setProperty("text", exported);
      QVERIFY(window.click("exportConfirm"));
      QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(exported), expected("writer-subset-edited-no-title.fountain"));
      QVERIFY(job->status().contains("Title page left out by choice."));
      // A write failure leaves the previous export complete.
      const auto locked = path("locked-folder");
      QVERIFY(QDir().mkpath(locked));
      replace_file(locked + "/script.fountain", "Existing Fountain evidence");
      QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
      const auto restore = qScopeGuard([&] { QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
      QVERIFY(job->request(locked + "/script.fountain"));
      QTRY_VERIFY(!job->busy());
      QVERIFY(job->status().startsWith("Fountain export failed:"));
      QVERIFY(job->status().endsWith("Any existing file there is unchanged."));
      QCOMPARE(bytes(locked + "/script.fountain"), QByteArray("Existing Fountain evidence"));
      // Neither the project folder nor the external original can be replaced.
      QVERIFY(!job->request(working + "/screenplay.json"));
      QVERIFY(job->status().contains("outside the project folder"));
      QVERIFY(!job->request(input));
      QVERIFY(job->status().contains("never replaces it"));
      QCOMPARE(bytes(input), bytes(APOLLO_IMPORT_FIXTURE_DIR "/writer-subset.fountain"));
    }
  }

  // Ordinary projects export too; leaving waits for an accepted export.
  void native_project_exports_fountain_and_leaving_waits_for_it() {
    const auto p = project();
    Window window(font_, path("session"), p.directory);
    QTRY_VERIFY(window.flow.document());
    auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
    QVERIFY(job && job->omissions().isEmpty());
    std::mutex mutex;
    std::condition_variable condition;
    bool released = false;
    job->set_worker_hook([&] { std::unique_lock lock(mutex); condition.wait(lock, [&] { return released; }); });
    const auto release = qScopeGuard([&] {
      { std::lock_guard lock(mutex); released = true; }
      condition.notify_all();
    });
    const auto output = path("native") + ".fountain";
    QVERIFY(job->request(output));
    window.type(QStringLiteral("Later "));
    QVERIFY(window.click("goHome"));
    QTRY_VERIFY(window.flow.leaving());
    bool heartbeat = false;
    QTimer::singleShot(0, [&] { heartbeat = true; });
    QTRY_VERIFY(heartbeat);
    QVERIFY(window.flow.document());
    { std::lock_guard lock(mutex); released = true; }
    condition.notify_all();
    QTRY_VERIFY(!window.flow.document());
    QCOMPARE(bytes(output), QByteArray("Current writing.\n\nSecond paragraph.\n"));
    QCOMPARE(open_project(p.directory).screenplay.elements.front().text, std::string("Later Current writing."));
  }

  // Enter before a space in dialogue leaves the space after the break. The
  // desk shows no layout error, and the saved writing reopens with the line
  // after the break at the dialogue column (profile: 216 pt), body slot 3.
  void deliberate_break_before_space_saves_and_reopens_without_layout_error() {
    const auto p = create_project(path("project"), {1, {{ElementType::cue, "MARA", false, {}, "cue"},
        {ElementType::dialogue, "I left the lights on for you. I always leave the lights on.", false, {}, "speech"}}});
    const std::string edited("I left the lights on for you.\n I always leave the lights on.");
    auto check = [&](Window& window) {
      const auto* layout = static_cast<const ScreenplayLayout*>(window.flow.document()->documentLayout());
      QVERIFY2(layout->error().isEmpty(), qPrintable(layout->error()));
      QVERIFY(!window.item("operationStatus")->property("text").toString().contains("QTextLayout"));
      const auto speech = window.flow.document()->lastBlock();
      QCOMPARE(block_element(speech).text, edited);
      QCOMPARE(speech.layout()->lineCount(), 2);
      // The second line begins after the break, with the space before the column.
      const auto line = speech.layout()->lineAt(1);
      QCOMPARE(line.textStart(), 30);
      const auto origin = layout->blockBoundingRect(speech).topLeft();
      QVERIFY(std::abs((origin.x() + line.cursorToX(31)) / px_per_point - 216) < 0.01);
      QVERIFY(std::abs((origin.y() + line.y() + line.ascent()) / px_per_point - (81 + 2 * profile::line_pitch)) < 0.01);
    };
    {
      Window window(font_, path("session"), p.directory);
      QTRY_VERIFY(window.flow.document());
      window.flow.editor()->setProperty("cursorPosition", window.flow.document()->lastBlock().position() + 29);
      QTest::keyClick(&window.view, Qt::Key_Return);
      check(window);
      if (QTest::currentTestFailed()) return;
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(open_project(p.directory).screenplay.elements.back().text, edited);
    }
    Window reopened(font_, path("session"), p.directory);
    QTRY_VERIFY(reopened.flow.document());
    check(reopened);
  }

  // Headings and cues typed in lowercase are drawn and exported in capitals;
  // the document, undo, autosave and reopen keep what was typed.
  void typed_case_is_kept_and_presented_in_capitals() {
    const auto p = create_project(path("project"), {1, {{ElementType::action, "Rain.", false, {}, "first"}}});
    const auto output = path("cased") + ".fountain";
    // Hand-written: the authored text with the heading and cue in capitals.
    const QByteArray expected("Rain.\n\nINT. DEPOT - DAY\n\nMara waits.\n\nMARA (V.O.)\nHere.\n");
    auto block = [](Window& window, int n) { return window.flow.document()->findBlockByNumber(n); };
    {
      Window window(font_, path("session"), p.directory);
      QTRY_VERIFY(window.flow.document());
      window.flow.editor()->setProperty("cursorPosition", 5);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QTest::keyClick(&window.view, Qt::Key_Backtab, Qt::ShiftModifier);
      window.type("int. depot - day");
      QCOMPARE(element_type(block(window, 1)), ElementType::heading);
      QCOMPARE(block_element(block(window, 1)).text, std::string("int. depot - day"));
      QVERIFY(draws(font_, block(window, 1), "INT. DEPOT - DAY"));
      // Editing inside the heading, then undoing it, keeps authored case.
      window.flow.editor()->setProperty("cursorPosition", block(window, 1).position() + 10);
      for (int i = 0; i < 5; ++i) QTest::keyClick(&window.view, Qt::Key_Backspace);
      window.type("Pier");
      QCOMPARE(block_element(block(window, 1)).text, std::string("int. Pier - day"));
      QVERIFY(draws(font_, block(window, 1), "INT. PIER - DAY"));
      for (int i = 0; i < 4 && block_element(block(window, 1)).text != "int. depot - day"; ++i)
        QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(block_element(block(window, 1)).text, std::string("int. depot - day"));
      QVERIFY(draws(font_, block(window, 1), "INT. DEPOT - DAY"));
      window.flow.editor()->setProperty("cursorPosition", block(window, 1).position() + block(window, 1).length() - 1);
      QTest::keyClick(&window.view, Qt::Key_Return);
      window.type("Mara waits.");
      QTest::keyClick(&window.view, Qt::Key_Return);
      QTest::keyClick(&window.view, Qt::Key_Tab);
      window.type("mara (v.o.)");
      QTest::keyClick(&window.view, Qt::Key_Return);
      window.type("Here.");
      QCOMPARE(element_type(block(window, 3)), ElementType::cue);
      QVERIFY(draws(font_, block(window, 3), "MARA (V.O.)"));
      QVERIFY(draws(font_, block(window, 2), "Mara waits."));
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      const auto saved = open_project(p.directory).screenplay.elements;
      QCOMPARE(saved.at(1).text, std::string("int. depot - day"));
      QCOMPARE(saved.at(3).text, std::string("mara (v.o.)"));
      // The export panel states the presentation; the file has the capitals.
      QVERIFY(window.click("exportButton"));
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(window.click("exportFormatFountain"));
      const auto eligibility = window.item("exportEligibility")->property("text").toString();
      QVERIFY2(eligibility.contains("Written in capitals, as Apollo shows and prints them: 1 scene heading and 1 character cue typed in lowercase."),
               qPrintable(eligibility));
      window.item("fountainDestination")->setProperty("text", output);
      QVERIFY(window.click("exportConfirm"));
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(output), expected);
      QVERIFY(job->status().contains("The project keeps the case as typed."));
    }
    QFile::remove(output);
    Window reopened(font_, path("session"), p.directory);
    QTRY_VERIFY(reopened.flow.document());
    QCOMPARE(block_element(block(reopened, 1)).text, std::string("int. depot - day"));
    QVERIFY(draws(font_, block(reopened, 1), "INT. DEPOT - DAY"));
    QVERIFY(draws(font_, block(reopened, 3), "MARA (V.O.)"));
    auto* job = qobject_cast<FountainExportJob*>(reopened.view.rootObject()->property("fountainJob").value<QObject*>());
    QVERIFY(job->request(output));
    QTRY_VERIFY(!job->busy());
    QCOMPARE(bytes(output), expected);
  }

  // docs/spacing.md with real keys in the Writing desk. Enter after a space,
  // then again on the new line, gives one speech with a space before the
  // break and a blank line. Undo/redo, autosave and reopen keep the writing
  // exactly; the export writes it as presented and says so. A speech that
  // begins with a blank line is explained and refused until corrected.
  void typed_spacing_is_kept_and_exported_as_presented() {
    const auto p = create_project(path("project"), {1, {{ElementType::action, "Rain.", false, {}, "first"}}});
    const auto output = path("spaced") + ".fountain";
    // Hand-written: no space at a line edge; the blank line as two spaces.
    const QByteArray expected("Rain.\n\nMARA\nAt the\n  \nsame time.\n");
    const std::string authored = "At the \n\nsame time.";
    auto block = [](Window& window, int n) { return window.flow.document()->findBlockByNumber(n); };
    auto layout_error = [](Window& window) {
      return static_cast<const ScreenplayLayout*>(window.flow.document()->documentLayout())->error();
    };
    {
      Window window(font_, path("session"), p.directory);
      QTRY_VERIFY(window.flow.document());
      window.flow.editor()->setProperty("cursorPosition", 5);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QTest::keyClick(&window.view, Qt::Key_Tab);
      window.type("MARA");
      QTest::keyClick(&window.view, Qt::Key_Return);
      window.type("At the same time.");
      QCOMPARE(element_type(block(window, 2)), ElementType::dialogue);
      window.flow.editor()->setProperty("cursorPosition", block(window, 2).position() + 7);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QTest::keyClick(&window.view, Qt::Key_Return);
      QCOMPARE(window.flow.document()->blockCount(), 3);
      QCOMPARE(block_element(block(window, 2)).text, authored);
      QVERIFY2(layout_error(window).isEmpty(), qPrintable(layout_error(window)));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(block_element(block(window, 2)).text, std::string("At the \nsame time."));
      QTest::keyClick(&window.view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      QCOMPARE(block_element(block(window, 2)).text, authored);
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
      QCOMPARE(open_project(p.directory).screenplay.elements.at(2).text, authored);
      QVERIFY(window.click("exportButton"));
      QTRY_VERIFY(window.popup_open("exportPanel"));
      QVERIFY(window.click("exportFormatFountain"));
      const auto eligibility = window.item("exportEligibility")->property("text").toString();
      QVERIFY2(eligibility.startsWith("Ready.") && eligibility.contains(
                   "Written without the spaces at the start or end of 1 line, which Apollo does not show or print. The project keeps them as typed."),
               qPrintable(eligibility));
      window.item("fountainDestination")->setProperty("text", output);
      QVERIFY(window.click("exportConfirm"));
      auto* job = qobject_cast<FountainExportJob*>(window.view.rootObject()->property("fountainJob").value<QObject*>());
      QTRY_VERIFY(!job->busy());
      QCOMPARE(bytes(output), expected);
      QVERIFY(job->status().contains("The project keeps them as typed."));
      QTest::keyClick(&window.view, Qt::Key_Escape);
      QTRY_VERIFY(window.flow.editor()->hasActiveFocus());
      window.flow.editor()->setProperty("cursorPosition", block(window, 2).position());
      QTest::keyClick(&window.view, Qt::Key_Return);
      QCOMPARE(block_element(block(window, 2)).text, "\n" + authored);
      QCOMPARE(job->check(true),
               QString("Element 3, Dialogue “At the ”: begins or ends with a blank line, which Fountain cannot keep in the speech."));
      QVERIFY(!job->request(output));
      QCOMPARE(bytes(output), expected);
      QTest::keyClick(&window.view, Qt::Key_Backspace);
      QCOMPARE(block_element(block(window, 2)).text, authored);
      QVERIFY(job->check(true).isEmpty());
      QTRY_COMPARE(window.flow.autosaver()->state(), QString("saved"));
    }
    QFile::remove(output);
    Window reopened(font_, path("session"), p.directory);
    QTRY_VERIFY(reopened.flow.document());
    QCOMPARE(block_element(block(reopened, 2)).text, authored);
    QVERIFY2(layout_error(reopened).isEmpty(), qPrintable(layout_error(reopened)));
    auto* job = qobject_cast<FountainExportJob*>(reopened.view.rootObject()->property("fountainJob").value<QObject*>());
    QVERIFY(job->request(output));
    QTRY_VERIFY(!job->busy());
    QCOMPARE(bytes(output), expected);
  }

  void unsupported_import_keeps_editable_control_and_publication_gated() {
    const auto input = path("unsupported-input"); replace_file(input, "@McKay\nA mixed-case cue.\n");
    Window window(font_, path("unsupported-state"), {}, {}, input);
    QTRY_VERIFY(window.flow.intake_preview()); QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.intake_edit_supported());
    QVERIFY(window.flow.intake_edit_blockers().contains("forced mixed-case character name requires review"));
    window.item("intakeProjectName")->setProperty("text", "Unsupported");
    window.item("intakeProjectParent")->setProperty("text", root_.path());
    QVERIFY(!window.item("editIntake")->isEnabled());
    window.flow.edit_import(root_.path(), "Unsupported");
    QVERIFY(!window.flow.document());
    QVERIFY(!QFileInfo::exists(root_.filePath("Unsupported")));
    QCOMPARE(bytes(input), QByteArray("@McKay\nA mixed-case cue.\n"));
  }
  void changed_published_writing_cannot_activate_as_the_reviewed_conversion() {
    const auto input = path("changed-conversion"); replace_file(input, "!Original writing.[[note]]");
    const auto state = path("changed-conversion-state");
    const auto previous = project();
    SessionState(state).save({previous.directory, previous.id, "first", 0, 0});
    const auto before = bytes(state);
    Window window(font_, state, {}, {}, input);
    QTRY_VERIFY(window.flow.intake_preview()); QTRY_VERIFY(!window.flow.busy());
    const auto parent = path("publication-parent"); QVERIFY(QDir().mkpath(parent));
    // Block GUI delivery while the real worker publishes, then replace only
    // the mutable screenplay with valid, unreviewed writing (same UUID/assets).
    window.flow.edit_import(parent, "Changed");
    const auto published = parent + "/Changed";
    QElapsedTimer deadline; deadline.start();
    while (!QFileInfo::exists(published + "/project.json") && deadline.elapsed() < 5000) QThread::msleep(1);
    QVERIFY(QFileInfo::exists(published + "/project.json"));
    const auto evidence = serialize_screenplay({0, {{ElementType::action, "Unreviewed replacement.", false, {}, "f1"}}});
    replace_file(published + "/screenplay.json", evidence);
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.document() && !window.flow.autosaver());
    QVERIFY(window.flow.error().contains("differs from the reviewed"));
    QCOMPARE(window.flow.location(), published);
    QCOMPARE(bytes(state), before);
    QVERIFY(ProjectLibrary(state + ".recents").load().empty());
    QCOMPARE(bytes(published + "/screenplay.json"), evidence);
    QCOMPARE(bytes(published + "/imports/original.fountain"), QByteArray("!Original writing.[[note]]"));
  }
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
  void interpretation_copy_uses_real_controls_preserves_old_reader_and_resumes() {
    const auto original = QByteArray("!A line. [[<b>note</b>]]\r\n\r\n/*keep*/\r\n\r\n[[bad\r\n\r\nVisible.\r\n");
    const auto input = path("interpret-source");
    replace_file(input, original);
    const auto captured = read_fountain_intake(input);
    const auto old = create_project(path("review-original"), {0, {{ElementType::action, ""}}}, captured.assets);
    const auto old_manifest = bytes(old.directory + "/project.json");
    const auto old_screenplay = bytes(old.screenplay_path());
    const auto state = path("interpret-state");
    QString published;
    {
      Window window(font_, state, old.directory);
      QTRY_VERIFY(window.flow.intake_active()); QTRY_VERIFY(!window.flow.busy());
      QVERIFY(!window.flow.intake_interpreted());
      window.item("intakeProjectName")->setProperty("text", "Interpreted");
      window.item("intakeProjectParent")->setProperty("text", root_.filePath("missing"));
      QVERIFY(window.click("interpretIntake"));
      QTRY_VERIFY(!window.flow.busy());
      QVERIFY(!window.flow.error().isEmpty());
      QVERIFY(window.flow.intake_active());
      QCOMPARE(window.flow.location(), old.directory);
      QVERIFY_THROWS_EXCEPTION(SourceError, ProjectSession(old.directory));
      window.item("intakeProjectParent")->setProperty("text", root_.path());
      QVERIFY(window.click("interpretIntake"));
      QTRY_VERIFY(window.flow.intake_interpreted()); QTRY_VERIFY(!window.flow.busy());
      published = window.flow.location();
      QVERIFY(published != old.directory);
      QVERIFY(!window.flow.document() && !window.flow.autosaver());
      QVERIFY(window.flow.intake_summary().contains("1 unresolved ranges"));
      QVERIFY(window.item("intakeFindingText")->property("text").toString().contains("[[bad"));
      const auto findings = window.flow.intake_findings();
      QVERIFY(std::any_of(findings.begin(), findings.end(), [](const QVariant& v) {
        const auto f = v.toMap(); return f.value("kind").toString().contains("Imported note 1") && f.value("text") == "<b>note</b>";
      }));
      QCOMPARE(SessionState(state).load()->directory, published);
      QCOMPARE(*open_project(published).assets.original_fountain, original);
      QVERIFY(window.view.grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/import-interpreted.png")));
      { ProjectSession released(old.directory); }
      QVERIFY(window.click("intakeHome")); QTRY_VERIFY(!window.flow.busy());
    }
    {
      Window restarted(font_, state);
      QTRY_VERIFY(restarted.flow.intake_interpreted()); QTRY_VERIFY(!restarted.flow.busy());
      QCOMPARE(restarted.flow.location(), published);
      QVERIFY(!restarted.flow.document());
    }
    QCOMPARE(bytes(old.directory + "/project.json"), old_manifest);
    QCOMPARE(bytes(old.screenplay_path()), old_screenplay);
    QCOMPARE(open_project(old.directory).assets, captured.assets);
    QCOMPARE(bytes(input), original);
  }
  void interpretation_refuses_changed_original_without_publishing_or_losing_reader() {
    const auto input = path("changed-source");
    replace_file(input, "Original. [[note]]");
    const auto captured = read_fountain_intake(input);
    const auto old = create_project(path("changed-review"), {0, {{ElementType::action, ""}}}, captured.assets);
    const auto state = path("changed-state");
    Window window(font_, state, old.directory);
    QTRY_VERIFY(window.flow.intake_active()); QTRY_VERIFY(!window.flow.busy());
    const auto session_before = bytes(state);
    replace_file(old.directory + "/imports/original.fountain", "Changed on disk.");
    window.flow.interpret_import(root_.path(), "Refused conversion");
    QTRY_VERIFY(!window.flow.busy());
    QVERIFY(!window.flow.error().isEmpty());
    QVERIFY(window.flow.intake_active());
    QVERIFY(!window.flow.intake_interpreted());
    QVERIFY(window.flow.intake_source().contains("Original. [[note]]"));
    QVERIFY(!QFileInfo::exists(root_.filePath("Refused conversion")));
    QCOMPARE(bytes(state), session_before);
    QCOMPARE(bytes(old.directory + "/imports/original.fountain"), QByteArray("Changed on disk."));
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
