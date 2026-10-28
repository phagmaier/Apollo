#include "editor/autosave.h"
#include "editor/editor_view.h"
#include "editor/pdf_export_job.h"
#include "editor/screenplay_layout.h"
#include "fountain/import.h"
#include "source/project.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickView>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QtTest>
#include <chrono>
#include <fstream>
#include <future>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

using namespace apollo;
using namespace apollo::editor;
using T = ElementType;
using Phase = Autosaver::Phase;

namespace {
std::string read(const QString& path) {
  std::ifstream input(path.toStdString());
  std::ostringstream text; text << input.rdbuf();
  return text.str();
}
QByteArray read_file(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
bool write_file(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
// The saved screenplay, read with Qt's JSON parser rather than Apollo's.
QJsonObject saved_json(const QString& project) {
  return QJsonDocument::fromJson(read_file(project + "/screenplay.json")).object();
}
QStringList saved_texts(const QString& project) {
  QStringList texts;
  for (const auto& element : saved_json(project).value("elements").toArray()) texts.push_back(element.toObject().value("text").toString());
  return texts;
}

// A thread-safe record of autosave phases.
struct Events {
  std::mutex mutex;
  std::vector<std::pair<Phase, std::uint64_t>> list;
  std::function<void(Phase, std::uint64_t)> also;
  Autosaver::Hook hook() {
    return [this](Phase phase, std::uint64_t revision) {
      { std::lock_guard lock(mutex); list.emplace_back(phase, revision); }
      if (also) also(phase, revision);
    };
  }
  std::vector<std::uint64_t> revisions(Phase phase) {
    std::lock_guard lock(mutex);
    std::vector<std::uint64_t> found;
    for (const auto& [at, revision] : list) if (at == phase) found.push_back(revision);
    return found;
  }
};

std::vector<std::string> ids(const QTextDocument& document) {
  std::vector<std::string> found;
  for (const auto& element : document_elements(document)) found.push_back(element.id);
  return found;
}
bool unique_ids(const QTextDocument& document) {
  const auto all = ids(document);
  return std::set<std::string>(all.begin(), all.end()).size() == all.size() &&
         std::none_of(all.begin(), all.end(), [](const std::string& id) { return id.empty(); });
}
}

class AutosaveTests : public QObject {
  Q_OBJECT
  QFont font_;
  QTemporaryDir directory_;
  QString project_path_;
  std::unique_ptr<QTextDocument> document_;
  std::unique_ptr<Autosaver> saver_;
  std::unique_ptr<QQuickView> view_;
  QQuickItem* editor_ = nullptr;

  // Opens the project in the Writing desk with autosave.
  bool open_project(Autosaver::Hook hook = {}) {
    view_.reset(); saver_.reset(); document_.reset();
    auto session = std::make_unique<source::ProjectSession>(project_path_);
    const auto& opened = session->opened();
    document_ = screenplay_document(opened.screenplay.elements, font_);
    saver_ = std::make_unique<Autosaver>(document_.get(), std::move(session));
    if (hook) saver_->set_hook(std::move(hook));
    view_ = std::make_unique<QQuickView>();
    editor_ = show_editor(*view_, *document_, nullptr, {}, saver_.get());
    saver_->guard_close(*view_);
    view_->resize(1000, 900);
    view_->show();
    if (!QTest::qWaitForWindowExposed(view_.get())) return false;
    view_->requestActivate();
    editor_->forceActiveFocus();
    return QTest::qWaitForWindowActive(view_.get()) && editor_->hasActiveFocus();
  }
  bool create(const std::vector<Element>& elements, std::uint64_t revision = 1) {
    project_path_ = directory_.filePath(QStringLiteral("project-%1").arg(++projects_));
    return !source::create_project(project_path_, {revision, elements}).id.isEmpty();
  }
  void type(const QString& text) {
    for (const auto character : text) QTest::keyClick(view_.get(), character.toLatin1());
  }
  void press(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) { QTest::keyClick(view_.get(), key, modifiers); }
  void place(int position) { editor_->setProperty("cursorPosition", position); }
  int position_of(const QString& text) const { return document_->toPlainText().indexOf(text); }
  bool saved() const { return saver_->state() == QStringLiteral("saved"); }
  int projects_ = 0;

  const std::vector<Element> scene{{T::heading, "INT. LAMP ROOM - NIGHT"}, {T::action, "Wind rattles the glass."},
                                   {T::cue, "MARA"}, {T::dialogue, "Hold the light steady."},
                                   {T::heading, "EXT. CLIFF - CONTINUOUS"}, {T::action, "Waves break below."}};

private slots:
  void initTestCase() {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    font_ = courier_prime(QStringLiteral(APOLLO_FONT_DIR));
    QVERIFY(directory_.isValid());
    qputenv("XDG_DATA_HOME", directory_.filePath("app-data").toUtf8());
    QVERIFY(QDir().mkpath(QStringLiteral(APOLLO_ARTIFACT_DIR)));
  }
  void init() { QTest::failOnWarning(QRegularExpression(QStringLiteral(".*"))); }
  void cleanup() {
    view_.reset(); saver_.reset(); document_.reset();
    QGuiApplication::clipboard()->clear();
  }

  // What is saved reopens as the same elements, ids, and layout: the
  // independent fixtures plus line breaks, Unicode, styles, and title fields.
  void source_and_document_round_trip() {
    std::vector<std::vector<Element>> cases;
    for (const char* name : {"01-element-geometry", "02-dialogue-split", "05-parenthetical-at-break", "06-action-split",
                             "07-title-page-and-manual-break", "08-dual-dialogue-and-contd"})
      cases.push_back(import_fountain(read(QStringLiteral(APOLLO_FIXTURE_DIR "/") + name + ".fountain")));
    cases.push_back(import_fountain(read(QStringLiteral(APOLLO_LOOK_SOURCE))));
    cases.push_back({{T::title, "Café *Noir*"}, {T::contact, "1 Rue de l'Église\nParis"},
                     {T::dialogue, "Line one\nline 𝄞 two", false, {{0, 4, Style::bold}, {19, 22, Style::italic}}},
                     {T::cue, ""}, {T::page_break, ""}, {T::action, "  "},
                     // Characters QTextDocument keeps as text.
                     {T::action, "tab\there \x01 nbsp\u00a0 object\ufffc"}});
    for (const auto& elements : cases) {
      const auto first = screenplay_document(elements, font_);
      const auto captured = document_elements(*first);
      QVERIFY(unique_ids(*first));
      const auto reopened_elements = source::parse_screenplay(source::serialize_screenplay({3, captured})).elements;
      QVERIFY(reopened_elements == source::canonical({3, captured}).elements);
      const auto reopened = screenplay_document(reopened_elements, font_);
      QVERIFY(source::canonical({3, document_elements(*reopened)}).elements == reopened_elements);
      for (std::size_t i = 0; i < elements.size(); ++i) {
        QCOMPARE(captured[i].text, elements[i].text);
        QCOMPARE(captured[i].type, elements[i].type);
      }
      const auto a = apollo::layout(captured), b = apollo::layout(reopened_elements);
      QCOMPARE(a.size(), b.size());
      for (std::size_t i = 0; i < a.size(); ++i) QVERIFY(a[i].text == b[i].text && a[i].page == b[i].page && a[i].slot == b[i].slot);
    }
  }

  // The Qt behavior that the deliberate-line-break representation relies on:
  // a QChar::LineSeparator stays inside its block as one position, survives
  // selection and undo, and QTextLayout always ends a line after it.
  void line_separator_probe() {
    const auto document = screenplay_document({{T::dialogue, "Line one\nline two", false, {}, "d1"}}, font_);
    QCOMPARE(document->blockCount(), 1);
    QCOMPARE(document->begin().text(), QStringLiteral("Line one") + QChar::LineSeparator + QStringLiteral("line two"));
    QCOMPARE(document_elements(*document).front().text, std::string("Line one\nline two"));
    QTextCursor cursor(document.get());
    cursor.setPosition(8);
    QVERIFY(cursor.movePosition(QTextCursor::NextCharacter));
    QCOMPARE(cursor.position(), 9);
    QCOMPARE(cursor.block(), document->begin());
    cursor.setPosition(5);
    cursor.setPosition(13, QTextCursor::KeepAnchor);
    QCOMPARE(cursor.selectedText(), QStringLiteral("one") + QChar::LineSeparator + QStringLiteral("line"));
    // QTextDocument::toPlainText() does not keep the distinction.
    QCOMPARE(document->toPlainText(), QStringLiteral("Line one\nline two"));
    cursor.setPosition(4);
    cursor.insertText(QString(QChar::LineSeparator));
    QCOMPARE(document->blockCount(), 1);
    QCOMPARE(document_elements(*document).front().text, std::string("Line\n one\nline two"));
    document->undo();
    QCOMPARE(document_elements(*document).front().text, std::string("Line one\nline two"));

    // The line breaks after the separator even when told the line is longer,
    // so the engine has to place breaks there for the editor to match it.
    QTextLayout layout(QStringLiteral("Line one") + QChar::LineSeparator + QStringLiteral("line two"), font_);
    layout.beginLayout();
    auto first = layout.createLine();
    first.setNumColumns(30);
    auto second = layout.createLine();
    second.setNumColumns(30);
    layout.endLayout();
    QCOMPARE(first.textLength(), 9);
    QCOMPARE(second.textStart(), 9);
    QCOMPARE(layout.lineForTextPosition(9).lineNumber(), 1);
    QCOMPARE(layout.lineForTextPosition(8).lineNumber(), 0);
  }

  // Splits give the part without the text's identity a new id; undo/redo
  // and scene moves keep every id (docs/source-format.md, "Identity").
  void identities_follow_edits() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    const auto original = ids(*document_);
    const auto action = document_->findBlock(position_of("Wind"));
    place(position_of("rattles"));
    press(Qt::Key_Return);
    QCOMPARE(document_->blockCount(), 7);
    QCOMPARE(block_element(action).id, original[1]);
    QCOMPARE(block_element(action).text, std::string("Wind "));
    QVERIFY(unique_ids(*document_));
    const auto split = ids(*document_);
    // Enter before a heading's text: the heading text keeps its id.
    place(position_of("EXT. CLIFF"));
    press(Qt::Key_Return);
    QCOMPARE(block_element(document_->findBlock(position_of("EXT. CLIFF"))).id, original[4]);
    QVERIFY(unique_ids(*document_));
    press(Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(ids(*document_) == split);
    press(Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(ids(*document_) == original);
    press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(ids(*document_) == split);
    // A whole-scene move keeps each element's id.
    place(position_of("EXT. CLIFF"));
    press(Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier);
    QCOMPARE(block_element(document_->begin()).text, std::string("EXT. CLIFF - CONTINUOUS"));
    auto moved = ids(*document_), sorted_split = split;
    std::sort(moved.begin(), moved.end());
    std::sort(sorted_split.begin(), sorted_split.end());
    QVERIFY(moved == sorted_split);
    // Pasted elements are new elements.
    QGuiApplication::clipboard()->setText(QStringLiteral("One.\nTwo."));
    place(position_of("Waves") + 5);
    press(Qt::Key_V, Qt::ControlModifier);
    QVERIFY(unique_ids(*document_));
    QCOMPARE(document_->blockCount(), 10);
  }

  // A block split outside the editor commands gets an identity as part of
  // its own undo step, before any capture or subsequent edit.
  void repairs_repeated_ids() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    const auto original = ids(*document_);
    QTextCursor cursor(document_.get());
    cursor.setPosition(position_of("rattles"));
    cursor.insertBlock();
    QVERIFY(unique_ids(*document_));
    const auto split = document_elements(*document_);
    saver_->flush();
    QVERIFY(unique_ids(*document_));
    QCOMPARE(block_element(document_->findBlock(position_of("Wind"))).id, original[1]);
    QTRY_VERIFY(saved());
    document_->undo();
    QVERIFY(ids(*document_) == original);
    document_->redo();
    QVERIFY(document_elements(*document_) == split);
  }

  // An IME-created block and the following typing are separate undo steps.
  // Saving between undo/redo must neither change their ids nor erase redo.
  void input_method_undo_redo_survives_autosave() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    const auto original = document_elements(*document_);
    place(position_of("rattles"));
    QInputMethodEvent commit;
    commit.setCommitString(QStringLiteral("\n"));
    QCoreApplication::sendEvent(editor_, &commit);
    QCOMPARE(document_->blockCount(), 7);
    type("x");
    QTRY_VERIFY(saved());
    const auto typed = document_elements(*document_);

    press(Qt::Key_Z, Qt::ControlModifier);
    QTRY_VERIFY(saved());
    QVERIFY(document_->isRedoAvailable());
    const auto split = document_elements(*document_);
    QVERIFY(unique_ids(*document_));
    press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(saved());
    QVERIFY(document_elements(*document_) == typed);
    press(Qt::Key_Z, Qt::ControlModifier);
    press(Qt::Key_Z, Qt::ControlModifier);
    QTRY_VERIFY(saved());
    QVERIFY(document_elements(*document_) == original);
    press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(saved());
    QVERIFY(document_elements(*document_) == split);
    press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(saved());
    QVERIFY(document_elements(*document_) == typed);
    QVERIFY(source::open_project(project_path_).screenplay.elements == source::canonical({0, typed}).elements);
  }

  // Edits through the real editor save and reopen as the same document.
  void edits_save_and_reopen() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    QCOMPARE(saver_->state(), QStringLiteral("saved"));
    const auto opened_revision = saver_->revision();
    place(position_of("Hold") + 4);
    type(" it");
    QCOMPARE(saver_->state(), QStringLiteral("saving"));
    press(Qt::Key_Return);
    type("Now");
    press(Qt::Key_Tab);
    place(position_of("Waves"));
    QGuiApplication::clipboard()->setText(QStringLiteral("Gulls scatter."));
    press(Qt::Key_V, Qt::ControlModifier);
    QTRY_VERIFY(saved());
    QVERIFY(saver_->revision() > opened_revision);
    QCOMPARE(saved_json(project_path_).value("revision").toInteger(), static_cast<qint64>(saver_->revision()));
    QVERIFY(saved_texts(project_path_).contains(QStringLiteral("Gulls scatter.Waves break below.")));
    const auto before = document_elements(*document_);
    const auto revision = saver_->revision();
    QVERIFY(open_project());
    QVERIFY(document_elements(*document_) == source::canonical({0, before}).elements);
    QCOMPARE(saver_->revision(), revision);
    QCOMPARE(saver_->state(), QStringLiteral("saved"));
  }

  // Exhausting the format's revision range must fail visibly, keeping both
  // the last reopenable file and the newer in-memory writing.
  void revision_exhaustion_preserves_last_valid_save() {
    QVERIFY(create(scene, 9007199254740990ULL));
    QVERIFY(open_project());
    place(position_of("glass.") + 6);
    type("x");
    QTRY_VERIFY(saved());
    QCOMPARE(saver_->saved_revision(), std::uint64_t(9007199254740991ULL));
    const auto before = read_file(project_path_ + "/screenplay.json");
    type("y");
    saver_->flush();
    QTRY_VERIFY(saver_->state() != QStringLiteral("saving"));
    QCOMPARE(saver_->state(), QStringLiteral("failed"));
    QVERIFY(saver_->status().contains("revision"));
    QCOMPARE(saver_->saved_revision(), std::uint64_t(9007199254740991ULL));
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), before);
    QVERIFY(document_->toPlainText().contains("glass.xy"));
    type("z");
    saver_->retry();
    view_.reset();
    saver_.reset(); // Drain the retry before checking the file again.
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), before);
    QVERIFY(document_->toPlainText().contains("glass.xyz"));
    const auto reopened = source::open_project(project_path_);
    QCOMPARE(reopened.screenplay.revision, std::uint64_t(9007199254740991ULL));
    QCOMPARE(reopened.screenplay.elements.at(1).text, std::string("Wind rattles the glass.x"));
  }

  // Continuous typing is captured within the interval of each first unsaved
  // change; captures do not wait for a pause.
  void continuous_typing_is_captured() {
    QVERIFY(create(scene));
    Events events;
    QVERIFY(open_project(events.hook()));
    place(position_of("glass.") + 6);
    QElapsedTimer clock;
    clock.start();
    std::vector<qint64> keys;
    while (clock.elapsed() < 1500) {
      type("a");
      keys.push_back(clock.elapsed());
      QTest::qWait(40);
    }
    // The first capture comes after the interval, not after typing stops.
    const auto captures = events.revisions(Phase::captured);
    QVERIFY2(captures.size() >= 3, qPrintable(QString::number(captures.size())));
    QTRY_VERIFY(saved());
    QVERIFY(saved_texts(project_path_).at(1).endsWith(QString(static_cast<qsizetype>(keys.size()), QLatin1Char('a'))));
  }

  // The writer writes captures in order, superseding queued ones; an older
  // completed save does not make later writing look saved.
  void slow_writes_stay_ordered() {
    QVERIFY(create(scene));
    Events events;
    std::atomic<bool> slow{true};
    QString state_at_first_ack;
    std::uint64_t saved_at_first_ack = 0;
    events.also = [&](Phase phase, std::uint64_t) {
      if (phase == Phase::writing && slow.exchange(false)) std::this_thread::sleep_for(std::chrono::milliseconds(700));
      // Acknowledgments arrive on the GUI thread, after the saved revision updates.
      if (phase == Phase::acknowledged && state_at_first_ack.isEmpty()) {
        state_at_first_ack = saver_->state();
        saved_at_first_ack = saver_->saved_revision();
      }
    };
    QVERIFY(open_project(events.hook()));
    place(position_of("glass.") + 6);
    type("x");
    saver_->flush();
    const auto first = saver_->revision();
    QTRY_VERIFY(!events.revisions(Phase::writing).empty());
    for (int i = 0; i < 3; ++i) {
      type("y");
      saver_->flush();
    }
    QTRY_VERIFY(!events.revisions(Phase::acknowledged).empty());
    QCOMPARE(events.revisions(Phase::acknowledged).front(), first);
    QCOMPARE(saved_at_first_ack, first);
    QCOMPARE(state_at_first_ack, QStringLiteral("saving"));
    QTRY_VERIFY(saved());
    const auto written = events.revisions(Phase::committed);
    QVERIFY(std::is_sorted(written.begin(), written.end()) && std::adjacent_find(written.begin(), written.end()) == written.end());
    // Two of the three later captures were superseded before writing.
    QCOMPARE(written.size(), std::size_t(2));
    QCOMPARE(written.back(), saver_->revision());
    QVERIFY(saved_texts(project_path_).at(1).endsWith("xyyy"));
  }

  // A destination change while idle must revoke Saved without waiting for an
  // edit. Retry, later edits, and shutdown must not overwrite the external file.
  void external_changes_preserve_both_versions_data() {
    QTest::addColumn<QString>("change");
    for (const auto* name : {"in-place", "replacement", "identical replacement", "deletion", "manifest",
                             "missing manifest", "missing lock", "folder replacement"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }
  void external_changes_preserve_both_versions() {
    QFETCH(QString, change);
    QVERIFY(create(scene));
    QVERIFY(open_project());
    const auto source_path = project_path_ + "/screenplay.json";
    const auto manifest_path = project_path_ + "/project.json";
    const auto original = read_file(source_path);
    const auto original_manifest = read_file(manifest_path);
    auto external = original;
    external.replace("Wind rattles the glass.", "An external writer was here.");
    if (change == "in-place") QVERIFY(write_file(source_path, external));
    if (change == "replacement") source::replace_file(source_path, external);
    if (change == "identical replacement") source::replace_file(source_path, original);
    if (change == "deletion") QVERIFY(QFile::remove(source_path));
    if (change == "manifest") QVERIFY(write_file(manifest_path, original_manifest + " \n"));
    if (change == "missing manifest") QVERIFY(QFile::remove(manifest_path));
    if (change == "missing lock") QVERIFY(QFile::remove(project_path_ + "/.apollo.lock"));
    if (change == "folder replacement") {
      QVERIFY(QDir().rename(project_path_, project_path_ + "-moved"));
      QVERIFY(QDir().mkpath(project_path_));
      QVERIFY(write_file(source_path, original));
      QVERIFY(write_file(manifest_path, original_manifest));
    }
    const auto expected = read_file(source_path), expected_manifest = read_file(manifest_path);
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    QVERIFY(saver_->status().contains("paused", Qt::CaseInsensitive));
    QVERIFY(document_->toPlainText().contains("Wind rattles the glass."));
    place(position_of("glass.") + 6);
    type(" My newer writing.");
    saver_->retry();
    QTest::qWait(Autosaver::capture_interval_ms + 300);
    QCOMPARE(saver_->state(), QStringLiteral("failed"));
    QVERIFY(document_->toPlainText().contains("glass. My newer writing."));
    view_.reset();
    saver_.reset();
    QCOMPARE(read_file(source_path), expected);
    QCOMPARE(read_file(manifest_path), expected_manifest);
    if (change == "deletion") QVERIFY(!QFile::exists(source_path));
  }

  // A watcher alone is too late when another writer changes the target
  // between capture and commit. The final check must discard our temporary.
  void external_change_during_write_is_not_replaced_data() {
    QTest::addColumn<int>("when");
    QTest::newRow("before write") << static_cast<int>(Phase::writing);
    QTest::newRow("before commit") << static_cast<int>(Phase::written);
    QTest::newRow("after commit") << static_cast<int>(Phase::committed);
  }
  void external_change_during_write_is_not_replaced() {
    QFETCH(int, when);
    QVERIFY(create(scene));
    const auto path = project_path_ + "/screenplay.json";
    auto external = read_file(path);
    external.replace("Wind rattles the glass.", "Changed during save.");
    Events events;
    events.also = [&](Phase phase, std::uint64_t) {
      if (static_cast<int>(phase) == when) source::replace_file(path, external);
    };
    QVERIFY(open_project(events.hook()));
    place(position_of("glass.") + 6);
    type(" My work.");
    saver_->flush();
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    QVERIFY(document_->toPlainText().contains("glass. My work."));
    view_.reset(); saver_.reset();
    QCOMPARE(read_file(path), external);
  }
  // A changed destination cancels both queued and uncaptured shutdown work.
  void conflict_cancels_queued_and_shutdown_saves() {
    QVERIFY(create(scene));
    const auto path = project_path_ + "/screenplay.json";
    auto external = read_file(path);
    external.replace("Wind rattles the glass.", "Keep the external version.");
    auto session = std::make_unique<source::ProjectSession>(project_path_);
    auto document = screenplay_document(session->opened().screenplay.elements, font_);
    std::promise<void> entered, resume;
    const auto ready = entered.get_future(), proceed = resume.get_future();
    std::atomic<bool> first{true};
    {
      Autosaver saver(document.get(), std::move(session));
      saver.set_hook([&](Phase phase, std::uint64_t) {
        if (phase == Phase::written && first.exchange(false)) {
          entered.set_value();
          proceed.wait();
        }
      });
      auto unblock = qScopeGuard([&] { resume.set_value(); });
      QTextCursor cursor(document.get());
      cursor.movePosition(QTextCursor::End);
      cursor.insertText(" My first edit.");
      saver.flush();
      QTRY_VERIFY(ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
      cursor.insertText(" My queued edit.");
      saver.flush();
      source::replace_file(path, external);
      resume.set_value();
      unblock.dismiss();
      QTRY_COMPARE(saver.state(), QStringLiteral("failed"));
      cursor.insertText(" My final uncaptured edit.");
    }
    QCOMPARE(read_file(path), external);
    QVERIFY(document->toPlainText().endsWith(" My first edit. My queued edit. My final uncaptured edit."));
  }

  void watches_survive_our_own_replacements() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    place(position_of("glass.") + 6);
    for (int i = 0; i < 3; ++i) {
      type("x");
      saver_->flush();
      QTRY_VERIFY(saved());
    }
    auto external = read_file(project_path_ + "/screenplay.json");
    external.replace("glass.xxx", "glass. External after three saves.");
    source::replace_file(project_path_ + "/screenplay.json", external);
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    QVERIFY(document_->toPlainText().contains("glass.xxx"));
  }

  // Copy the current writing, not the last disk version; switch only the
  // destination and retain Qt's cursor, identities and undo history.
  void save_a_copy_becomes_active() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    const auto original = read_file(project_path_ + "/screenplay.json");
    const auto old_id = source::open_project(project_path_).project.id;
    auto external = original;
    external.replace("Wind rattles the glass.", "An external version.");
    source::replace_file(project_path_ + "/screenplay.json", external);
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    place(position_of("glass.") + 6);
    type(" My work.");
    const auto expected = document_elements(*document_);
    const auto position = editor_->property("cursorPosition").toInt();
    QVERIFY(QMetaObject::invokeMethod(editor_, "select", Q_ARG(int, position - 8), Q_ARG(int, position)));
    QSignalSpy failed(saver_.get(), &Autosaver::copyCompleted);
    QVERIFY(saver_->save_copy(project_path_)); // Collides while original is paused.
    QTRY_COMPARE(failed.size(), 1);
    QVERIFY(!failed.front().front().toBool());
    QCOMPARE(saver_->state(), QStringLiteral("failed"));
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), external);
    auto* panel = view_->rootObject()->findChild<QObject*>("copyPanel");
    QVERIFY(panel);
    QTRY_VERIFY(panel->property("visible").toBool());
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/copy-failed.png"));
    press(Qt::Key_Escape);
    QTRY_VERIFY(!panel->property("visible").toBool());
    const auto copy = project_path_ + " copy";
    QSignalSpy completed(saver_.get(), SIGNAL(copyCompleted(bool)));
    auto* button = view_->rootObject()->findChild<QQuickItem*>("saveCopy");
    QVERIFY(button && button->isVisible());
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, button->mapToScene(button->boundingRect().center()).toPoint());
    auto* destination = view_->rootObject()->findChild<QObject*>("copyDestination");
    QVERIFY(destination);
    destination->setProperty("text", copy);
    auto* confirm = view_->rootObject()->findChild<QQuickItem*>("copyConfirm");
    QVERIFY(confirm);
    QTRY_VERIFY(confirm->isVisible());
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, confirm->mapToScene(confirm->boundingRect().center()).toPoint());
    QTRY_COMPARE(completed.size(), 1);
    QVERIFY(completed.front().front().toBool());
    QCOMPARE(saver_->location(), copy);
    QTRY_VERIFY(saved());
    QVERIFY(source::open_project(copy).project.id != old_id);
    QVERIFY(source::open_project(copy).screenplay.elements == expected);
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), external);
    QCOMPARE(editor_->property("cursorPosition").toInt(), position);
    QCOMPARE(editor_->property("selectionStart").toInt(), position - 8);
    QCOMPARE(editor_->property("selectionEnd").toInt(), position);
    auto* location = view_->rootObject()->findChild<QQuickItem*>("projectLocation");
    QVERIFY(location);
    QCOMPARE(location->property("text").toString(), copy);
    QCOMPARE(view_->rootObject()->property("exportDestination").toString(), copy + ".pdf");
    place(position);
    press(Qt::Key_Z, Qt::ControlModifier);
    QTRY_VERIFY(saved());
    QVERIFY(!saved_texts(copy).at(1).endsWith(" My work."));
    press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    type(" Later.");
    QTRY_VERIFY(saved());
    QCOMPARE(saved_texts(copy).at(1), QStringLiteral("Wind rattles the glass. My work. Later."));
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), external);
    source::ProjectSession released(project_path_);
    QVERIFY_THROWS_EXCEPTION(source::SourceError, source::ProjectSession{copy});
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/copy-active.png"));
  }

  void destruction_during_copy_preserves_later_writing() {
    QVERIFY(create(scene));
    auto session = std::make_unique<source::ProjectSession>(project_path_);
    auto document = screenplay_document(session->opened().screenplay.elements, font_);
    std::promise<void> entered, resume;
    const auto ready = entered.get_future(), proceed = resume.get_future();
    const auto copy = project_path_ + " copy";
    {
      Autosaver saver(document.get(), std::move(session));
      saver.set_hook([&](Phase phase, std::uint64_t) {
        if (phase == Phase::copying) { entered.set_value(); proceed.wait(); }
      });
      auto unblock = qScopeGuard([&] { resume.set_value(); });
      QTextCursor cursor(document.get());
      cursor.movePosition(QTextCursor::End);
      cursor.insertText(" Copied.");
      QVERIFY(saver.save_copy(copy));
      QTRY_VERIFY(ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
      cursor.insertText(" During copy.");
      resume.set_value();
      unblock.dismiss();
    }
    QCOMPARE(saved_texts(copy).last(), QStringLiteral("Waves break below. Copied. During copy."));
  }

  void copy_orders_in_flight_and_queued_saves() {
    QVERIFY(create(scene));
    auto session = std::make_unique<source::ProjectSession>(project_path_);
    auto document = screenplay_document(session->opened().screenplay.elements, font_);
    std::promise<void> entered, resume;
    const auto ready = entered.get_future(), proceed = resume.get_future();
    std::atomic<bool> first{true};
    Autosaver saver(document.get(), std::move(session));
    saver.set_hook([&](Phase phase, std::uint64_t) {
      if (phase == Phase::written && first.exchange(false)) { entered.set_value(); proceed.wait(); }
    });
    auto unblock = qScopeGuard([&] { resume.set_value(); });
    QTextCursor cursor(document.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(" First.");
    saver.flush();
    QTRY_VERIFY(ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    cursor.insertText(" Queued.");
    saver.flush();
    cursor.insertText(" Uncaptured.");
    const auto copy = project_path_ + " copy";
    const auto alias = project_path_ + "-parent";
    QVERIFY(QFile::link(directory_.path(), alias));
    QVERIFY(saver.save_copy(alias + '/' + QFileInfo(copy).fileName()));
    cursor.insertText(" After request.");
    resume.set_value();
    unblock.dismiss();
    QTRY_COMPARE(saver.location(), copy);
    QTRY_COMPARE(saver.state(), QStringLiteral("saved"));
    QCOMPARE(saved_texts(project_path_).last(), QStringLiteral("Waves break below. First."));
    QCOMPARE(saved_texts(copy).last(), QStringLiteral("Waves break below. First. Queued. Uncaptured. After request."));
  }

  void changed_or_locked_copy_does_not_activate_data() {
    QTest::addColumn<bool>("locked");
    QTest::newRow("changed before activation") << false;
    QTest::newRow("locked before activation") << true;
  }
  void changed_or_locked_copy_does_not_activate() {
    QFETCH(bool, locked);
    QVERIFY(create(scene));
    auto session = std::make_unique<source::ProjectSession>(project_path_);
    auto document = screenplay_document(session->opened().screenplay.elements, font_);
    const auto copy = project_path_ + " copy";
    std::unique_ptr<source::ProjectSession> competitor;
    Autosaver saver(document.get(), std::move(session));
    saver.set_hook([&](Phase phase, std::uint64_t) {
      if (phase != Phase::copied) return;
      if (locked) competitor = std::make_unique<source::ProjectSession>(copy);
      else {
        auto external = read_file(copy + "/screenplay.json");
        external.replace("Wind rattles the glass.", "External copy writing.");
        source::replace_file(copy + "/screenplay.json", external);
      }
    });
    QTextCursor cursor(document.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(" Retained.");
    QSignalSpy completed(&saver, &Autosaver::copyCompleted);
    QVERIFY(saver.save_copy(copy));
    QTRY_COMPARE(completed.size(), 1);
    QVERIFY(!completed.front().front().toBool());
    QCOMPARE(saver.location(), project_path_);
    QVERIFY(saver.copy_error().contains("could not be activated"));
    QTRY_COMPARE(saver.state(), QStringLiteral("saved"));
    QCOMPARE(saved_texts(project_path_).last(), QStringLiteral("Waves break below. Retained."));
    QCOMPARE(saved_texts(copy).at(1), locked ? QStringLiteral("Wind rattles the glass.") : QStringLiteral("External copy writing."));
  }

  void copy_from_missing_original_keeps_imports_and_later_edits() {
    QVERIFY(create(scene));
    const QByteArray material("{\"note\":\"Keep this\"}\n"), original("Title: Original\r\n\r\nINT. ROOM - DAY\r\n");
    QVERIFY(write_file(project_path_ + "/imported-material.json", material));
    QVERIFY(QDir().mkpath(project_path_ + "/imports"));
    QVERIFY(write_file(project_path_ + "/imports/original.fountain", original));
    QVERIFY(open_project());
    const auto moved = project_path_ + "-moved";
    QVERIFY(QDir().rename(project_path_, moved));
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    const auto old_source = read_file(moved + "/screenplay.json");
    std::promise<void> entered, resume;
    const auto ready = entered.get_future(), proceed = resume.get_future();
    saver_->set_hook([&](Phase phase, std::uint64_t) {
      if (phase == Phase::copying) { entered.set_value(); proceed.wait(); }
    });
    auto unblock = qScopeGuard([&] { resume.set_value(); });
    place(position_of("glass.") + 6);
    type(" First.");
    const auto copy = project_path_ + " copy";
    QVERIFY(saver_->save_copy(copy));
    QTRY_VERIFY(ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    QVERIFY(saver_->copy_busy());
    QVERIFY(!saver_->save_copy(project_path_ + " duplicate"));
    type(" Later.");
    QTest::qWait(Autosaver::capture_interval_ms + 50);
    QVERIFY(!saved());
    view_->close();
    QVERIFY(view_->isVisible()); // Closing waits for copy and later edits.
    resume.set_value();
    unblock.dismiss();
    QTRY_COMPARE(saver_->location(), copy);
    QTRY_VERIFY(saved());
    QTRY_VERIFY(!view_->isVisible());
    QCOMPARE(saved_texts(copy).at(1), QStringLiteral("Wind rattles the glass. First. Later."));
    QCOMPARE(read_file(copy + "/imported-material.json"), material);
    QCOMPARE(read_file(copy + "/imports/original.fountain"), original);
    QCOMPARE(read_file(moved + "/screenplay.json"), old_source);
    QVERIFY(!QFile::exists(moved + "/.apollo.lock"));
  }

  void failed_copy_keeps_original_active_data() {
    QTest::addColumn<QString>("failure");
    for (const auto* failure : {"collision", "empty folder", "missing parent", "read-only parent", "symlink", "relative path"})
      QTest::newRow(failure) << QString::fromLatin1(failure);
  }
  void failed_copy_keeps_original_active() {
    QFETCH(QString, failure);
    QVERIFY(create(scene));
    QVERIFY(open_project());
    auto copy = project_path_ + " copy";
    if (failure == "collision") {
      QVERIFY(QDir().mkpath(copy));
      QVERIFY(write_file(copy + "/keep.txt", "mine"));
    }
    if (failure == "empty folder") QVERIFY(QDir().mkpath(copy));
    if (failure == "missing parent") copy += "/missing/project";
    if (failure == "read-only parent") {
      QVERIFY(QDir().mkpath(copy));
      QVERIFY(QFile::setPermissions(copy, QFile::ReadOwner | QFile::ExeOwner));
      copy += "/project";
    }
    auto restore = qScopeGuard([&] {
      if (failure == "read-only parent") QFile::setPermissions(QFileInfo(copy).absolutePath(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    });
    if (failure == "symlink") QVERIFY(QFile::link(project_path_, copy));
    if (failure == "relative path") copy = "relative-project";
    place(position_of("glass.") + 6);
    type(" Retain me.");
    const auto expected = document_elements(*document_);
    QSignalSpy completed(saver_.get(), &Autosaver::copyCompleted);
    if (failure == "relative path") QVERIFY(!saver_->save_copy(copy));
    else {
      QVERIFY(saver_->save_copy(copy));
      QTRY_COMPARE(completed.size(), 1);
      QVERIFY(!completed.front().front().toBool());
    }
    QCOMPARE(saver_->location(), project_path_);
    QVERIFY(!saver_->copy_error().isEmpty());
    QVERIFY(document_elements(*document_) == expected);
    QTRY_VERIFY(saved());
    QCOMPARE(saved_texts(project_path_).at(1), QStringLiteral("Wind rattles the glass. Retain me."));
    if (failure == "collision") QCOMPARE(read_file(copy + "/keep.txt"), QByteArray("mine"));
    if (failure == "empty folder") QVERIFY(QDir(copy).isEmpty());
    QVERIFY(QDir(directory_.path()).entryList({".apollo-staging-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
  }

  void writing_during_a_slow_retry_is_saved() {
    QVERIFY(create(scene));
    auto session = std::make_unique<source::ProjectSession>(project_path_);
    auto document = screenplay_document(session->opened().screenplay.elements, font_);
    std::promise<void> entered, resume;
    const auto ready = entered.get_future(), proceed = resume.get_future();
    std::atomic<bool> first{true};
    Autosaver saver(document.get(), std::move(session));
    auto unblock = qScopeGuard([&] { resume.set_value(); });
    const auto moved = project_path_ + "-moved";
    QVERIFY(QDir().rename(project_path_, moved));
    QTRY_COMPARE(saver.state(), QStringLiteral("failed"));
    QTextCursor cursor(document.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(" First.");
    QVERIFY(QDir().rename(moved, project_path_));
    saver.set_hook([&](Phase phase, std::uint64_t) {
      if (phase == Phase::writing && first.exchange(false)) {
        entered.set_value();
        proceed.wait();
      }
    });
    saver.retry();
    QTRY_VERIFY(ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    cursor.insertText(" Later.");
    QTest::qWait(Autosaver::capture_interval_ms + 50);
    resume.set_value();
    unblock.dismiss();
    QTRY_COMPARE(saver.state(), QStringLiteral("saved"));
    QCOMPARE(saved_texts(project_path_).last(), QStringLiteral("Waves break below. First. Later."));
  }

  void failed_saves_keep_writing_data() {
    QTest::addColumn<QString>("failure");
    QTest::newRow("short write") << "short write";
    QTest::newRow("read-only folder") << "read-only folder";
    QTest::newRow("missing folder") << "missing folder";
  }
  // A failed save keeps the old file and the writing, reports the failure,
  // and Retry saves once the destination works again.
  void failed_saves_keep_writing() {
    QFETCH(QString, failure);
    QVERIFY(create(scene));
    Events events;
    std::atomic<bool> inject{false};
    events.also = [&](Phase phase, std::uint64_t) {
      if (phase == Phase::written && inject) throw std::runtime_error("injected short write");
    };
    QVERIFY(open_project(events.hook()));
    const auto before = read_file(project_path_ + "/screenplay.json");
    const auto moved = project_path_ + "-moved";
    if (failure == "short write") inject = true;
    if (failure == "read-only folder") QVERIFY(QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::ExeOwner));
    if (failure == "missing folder") QVERIFY(QDir().rename(project_path_, moved));
    place(position_of("glass.") + 6);
    type(" Again.");
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    QVERIFY(saver_->status().startsWith(QStringLiteral("Not saved")));
    QVERIFY2(saver_->status().contains(project_path_), qPrintable(saver_->status()));
    auto* retry = view_->rootObject()->findChild<QQuickItem*>("retrySave");
    QVERIFY(retry && retry->isVisible());
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/save-failed-%1.png").arg(QString(failure).replace(' ', '-')));
    QVERIFY(document_->toPlainText().contains("glass. Again."));
    QCOMPARE(read_file((failure == "missing folder" ? moved : project_path_) + "/screenplay.json"), before);
    // More typing keeps failing rather than clearing the error.
    type("!");
    QTest::qWait(Autosaver::capture_interval_ms + 300);
    QCOMPARE(saver_->state(), QStringLiteral("failed"));
    inject = false;
    if (failure == "missing folder") QVERIFY(QDir().rename(moved, project_path_));
    QVERIFY(QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, retry->mapToScene(retry->boundingRect().center()).toPoint());
    QTRY_VERIFY(saved());
    QVERIFY(!retry->isVisible());
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/saved.png"));
    QVERIFY(saved_texts(project_path_).contains("Wind rattles the glass. Again.!"));
  }

  void reload_preserves_memory_before_adopting_external_writing() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    auto external = read_file(project_path_ + "/screenplay.json");
    external.replace("Wind rattles the glass.", "External writing.");
    source::replace_file(project_path_ + "/screenplay.json", external);
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    place(position_of("glass.") + 6);
    type(" My unsaved work.");
    QSignalSpy completed(saver_.get(), &Autosaver::reloadCompleted);
    auto* reload = view_->rootObject()->findChild<QQuickItem*>("reloadProject");
    QVERIFY(reload && reload->isVisible());
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, reload->mapToScene(reload->boundingRect().center()).toPoint());
    auto* panel = view_->rootObject()->findChild<QObject*>("reloadPanel");
    QVERIFY(panel);
    QTRY_VERIFY(panel->property("opened").toBool());
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/reload-confirm.png"));
    auto* confirm = view_->rootObject()->findChild<QQuickItem*>("reloadConfirm");
    QVERIFY(confirm && confirm->isVisible());
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, confirm->mapToScene(confirm->boundingRect().center()).toPoint());
    QTRY_COMPARE(completed.size(), 1);
    QVERIFY(completed.front().front().toBool());
    QCOMPARE(saved_texts(project_path_).at(1), QStringLiteral("External writing."));
    QCOMPARE(document_->toPlainText().contains("External writing."), true);
    QVERIFY(document_elements(*document_) == source::open_project(project_path_).screenplay.elements);
    QVERIFY(!document_->isUndoAvailable());
    QCOMPARE(saver_->state(), QStringLiteral("saved"));
    const auto origin = source::open_project(project_path_).project;
    source::RecoveryStore recovery(origin, {}, source::RecoveryStore::emergency_directory(origin));
    const auto copies = recovery.verified();
    QVERIFY(!copies.empty());
    QVERIFY(saved_texts(copies.front().directory).contains("Wind rattles the glass. My unsaved work."));
    place(position_of("External writing.") + 17);
    type(" After reload.");
    QTRY_VERIFY(saved());
    QVERIFY(saved_texts(project_path_).contains("External writing. After reload."));
  }

  void invalid_reload_keeps_memory_and_external_evidence() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    QVERIFY(write_file(project_path_ + "/screenplay.json", "damaged source"));
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    place(position_of("glass.") + 6);
    type(" Retained.");
    const auto expected = document_elements(*document_);
    QSignalSpy completed(saver_.get(), &Autosaver::reloadCompleted);
    QVERIFY(saver_->reload());
    QTRY_COMPARE(completed.size(), 1);
    QVERIFY(!completed.front().front().toBool());
    QVERIFY(document_elements(*document_) == expected);
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), QByteArray("damaged source"));
    QVERIFY(saver_->current_protected());
    QCOMPARE(saver_->state(), QStringLiteral("failed"));
  }

  void later_writing_immediately_revokes_visible_emergency_coverage() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    QVERIFY(write_file(project_path_ + "/screenplay.json", "damaged source"));
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    place(position_of("glass.") + 6);
    type(" Protected.");
    saver_->flush();
    QTRY_VERIFY(saver_->current_protected());
    auto* status = view_->rootObject()->findChild<QObject*>("saveStatus");
    QVERIFY(status);
    QVERIFY(status->property("text").toString().contains("Current writing protected"));
    type("x");
    QVERIFY(!saver_->current_protected());
    // Check the actual QML binding before another capture can acknowledge it.
    QVERIFY(status->property("text").toString().contains("latest writing is unprotected"));
  }

  void pdf_export_result_remains_visible_after_reload() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    QSignalSpy reloaded(saver_.get(), &Autosaver::reloadCompleted);
    QVERIFY(saver_->reload());
    QTRY_COMPARE(reloaded.size(), 1);
    QVERIFY(reloaded.front().front().toBool());
    PdfExportJob job(document_.get(), QStringLiteral(APOLLO_FONT_DIR), QStringLiteral("/nonexistent/pdf-export"));
    view_->rootObject()->setProperty("exportJob", QVariant::fromValue<QObject*>(&job));
    auto* footer = view_->rootObject()->findChild<QObject*>("operationStatus");
    QVERIFY(footer);
    QVERIFY(footer->property("text").toString().contains("Previous writing preserved"));
    QSignalSpy exported(&job, &PdfExportJob::completed);
    QVERIFY(job.request(directory_.filePath("after-reload.pdf")));
    QTRY_COMPARE(exported.size(), 1);
    QVERIFY(!exported.front().front().toBool());
    QVERIFY(job.status().contains("failed"));
    QCOMPARE(footer->property("text").toString(), job.status());
    view_->rootObject()->setProperty("exportJob", QVariant::fromValue<QObject*>(nullptr));
  }

  void reload_and_close_when_both_destinations_fail() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    auto external = read_file(project_path_ + "/screenplay.json");
    external.replace("Wind rattles the glass.", "External writing.");
    source::replace_file(project_path_ + "/screenplay.json", external);
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    // Deterministic emergency-storage failure without filling a filesystem.
    saver_->set_hook([](Phase phase, std::uint64_t) {
      if (phase == Phase::recovering) throw std::runtime_error("emergency storage unavailable");
    });
    place(position_of("glass.") + 6);
    type(" Not protected.");
    const auto expected = document_elements(*document_);
    QSignalSpy completed(saver_.get(), &Autosaver::reloadCompleted);
    QVERIFY(saver_->reload());
    QTRY_COMPARE(completed.size(), 1);
    QVERIFY(!completed.front().front().toBool());
    QVERIFY(document_elements(*document_) == expected);
    QVERIFY(document_->isUndoAvailable());
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), external);
    QVERIFY(!saver_->current_protected());
    view_->close();
    QVERIFY(view_->isVisible());
    QTRY_VERIFY(saver_->recovery_status().contains("emergency storage unavailable"));
    auto* close = view_->rootObject()->findChild<QQuickItem*>("closeExplicit");
    QVERIFY(close);
    QTRY_VERIFY(close->isVisible());
    QCOMPARE(close->property("text").toString(), QStringLiteral("Discard Unprotected Writing and Close"));
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/close-unprotected.png"));
    saver_->keep_open();
    QVERIFY(view_->isVisible());
    QVERIFY(!saver_->close_dialog());
    view_->close();
    QTRY_VERIFY(saver_->close_dialog());
    saver_->close_with_copy_or_discard();
    QVERIFY(!view_->isVisible());
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), external);
  }

  void writing_during_reload_cancels_adoption_and_is_protected() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    auto external = read_file(project_path_ + "/screenplay.json");
    external.replace("Wind rattles the glass.", "External writing.");
    source::replace_file(project_path_ + "/screenplay.json", external);
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    std::promise<void> entered, resume;
    const auto ready = entered.get_future(), proceed = resume.get_future();
    saver_->set_hook([&](Phase phase, std::uint64_t) {
      if (phase == Phase::reloading) { entered.set_value(); proceed.wait(); }
    });
    auto unblock = qScopeGuard([&] { resume.set_value(); });
    QSignalSpy completed(saver_.get(), &Autosaver::reloadCompleted);
    QVERIFY(saver_->reload());
    QTRY_VERIFY(ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    QVERIFY(!editor_->isEnabled());
    // Direct document mutation tests a producer other than keyboard input.
    QTextCursor cursor(document_.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(" Later writing.");
    QTest::qWait(Autosaver::capture_interval_ms + 30);
    resume.set_value();
    unblock.dismiss();
    QTRY_COMPARE(completed.size(), 1);
    QVERIFY(!completed.front().front().toBool());
    QVERIFY(document_->toPlainText().contains("Later writing."));
    QTRY_VERIFY(saver_->current_protected());
    saver_->retry();
    QTest::qWait(100);
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), external);
  }

  void retry_from_failure_close_finishes_closing() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    QVERIFY(QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::ExeOwner));
    auto restore = qScopeGuard([&] { QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    place(position_of("glass.") + 6);
    type(" Finish.");
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    view_->close();
    QTRY_VERIFY(saver_->close_dialog());
    QVERIFY(QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    saver_->retry();
    QTRY_VERIFY(!view_->isVisible());
    QVERIFY(saved());
    QCOMPARE(saved_texts(project_path_).at(1), QStringLiteral("Wind rattles the glass. Finish."));
  }

  void failed_save_is_visible_while_emergency_io_is_slow() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    const auto before = read_file(project_path_ + "/screenplay.json");
    std::promise<void> entered, resume;
    const auto ready = entered.get_future(), proceed = resume.get_future();
    std::atomic<bool> first{true};
    saver_->set_hook([&](Phase phase, std::uint64_t) {
      if (phase == Phase::written) throw std::runtime_error("project storage failed");
      if (phase == Phase::recovering && first.exchange(false)) { entered.set_value(); proceed.wait(); }
    });
    auto unblock = qScopeGuard([&] { resume.set_value(); });
    place(position_of("glass.") + 6);
    type(" Keep this.");
    saver_->flush();
    QTRY_VERIFY(ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    QVERIFY(saver_->status().contains("project storage failed", Qt::CaseInsensitive));
    QVERIFY(!saver_->current_protected());
    view_->close();
    QTRY_VERIFY(saver_->close_dialog());
    QVERIFY(view_->isVisible());
    saver_->keep_open();
    resume.set_value();
    unblock.dismiss();
    QTRY_VERIFY(saver_->current_protected());
    QCOMPARE(saver_->state(), QStringLiteral("failed"));
    QCOMPARE(read_file(project_path_ + "/screenplay.json"), before);
  }

  // Closing waits for the save; while saving fails, the first close is
  // refused until the owner chooses a protected close or explicit discard.
  void close_waits_for_saving() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    place(position_of("glass.") + 6);
    type(" Last.");
    view_->close();
    QVERIFY(view_->isVisible());
    QTRY_VERIFY(!view_->isVisible());
    QVERIFY(saved());
    QVERIFY(saved_texts(project_path_).contains("Wind rattles the glass. Last."));

    QVERIFY(open_project());
    QVERIFY(QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::ExeOwner));
    auto restore = qScopeGuard([&] { QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    place(position_of("glass.") + 6);
    type("!");
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    view_->close();
    QVERIFY(view_->isVisible());
    QTRY_VERIFY(saver_->close_dialog());
    QTRY_VERIFY(saver_->current_protected());
    QVERIFY(saver_->status().contains("Current writing protected"));
    view_->close();
    QVERIFY(view_->isVisible()); // Repeated close never implicitly discards.
    auto* close = view_->rootObject()->findChild<QQuickItem*>("closeExplicit");
    QVERIFY(close);
    QTRY_VERIFY(close->isVisible());
    auto* panel = view_->rootObject()->findChild<QObject*>("failurePanel");
    QVERIFY(panel);
    QTRY_VERIFY(panel->property("opened").toBool());
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/close-protected.png"));
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, close->mapToScene(close->boundingRect().center()).toPoint());
    QVERIFY(!view_->isVisible());
    QVERIFY(QFile::setPermissions(project_path_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  }

  // Destroying the autosaver writes what it has not yet captured.
  void destruction_writes_pending_changes() {
    QVERIFY(create(scene));
    QVERIFY(open_project());
    place(position_of("glass.") + 6);
    type(" Gone.");
    view_.reset();
    saver_.reset();
    QVERIFY(saved_texts(project_path_).contains("Wind rattles the glass. Gone."));
  }
};

QTEST_MAIN(AutosaveTests)
#include "autosave_tests.moc"
