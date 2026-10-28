#include "editor/autosave.h"
#include "editor/editor_view.h"
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
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QtTest>
#include <chrono>
#include <fstream>
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
    const auto opened = source::open_project(project_path_);
    document_ = screenplay_document(opened.screenplay.elements, font_);
    saver_ = std::make_unique<Autosaver>(document_.get(), opened.project, opened.screenplay.revision);
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
    QVERIFY2(saver_->status().contains("screenplay.json"), qPrintable(saver_->status()));
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

  // Closing waits for the save; while saving fails, the first close is
  // refused with a warning and a second discards.
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
    place(position_of("glass.") + 6);
    type("!");
    QTRY_COMPARE(saver_->state(), QStringLiteral("failed"));
    view_->close();
    QVERIFY(view_->isVisible());
    QVERIFY(saver_->status().contains("Close again to discard"));
    view_->close();
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
