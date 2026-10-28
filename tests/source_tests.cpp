#include "source/project.h"
#include "source/screenplay_format.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <limits>
#include <unistd.h>

using namespace apollo;
using namespace apollo::source;
using T = ElementType;

namespace {
QByteArray read_file(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
bool write_file(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

// fixtures/source/v1-example.screenplay.json, written out by hand. Its
// offsets count code points; these count UTF-8 bytes: "Café " is 5 code
// points and 6 bytes, and "Line one\nline 𝄞 " is 16 code points and 19 bytes.
Screenplay example() {
  return {42, {{T::title, "The Last Broadcast", false, {}, "t1"},
               {T::author, "Ana Núñez", false, {}, "t2"},
               {T::contact, "1 Rue de l'Église\nParis", false, {}, "t3"},
               {T::heading, "INT. LIGHTHOUSE — NIGHT", false, {}, "e1"},
               {T::action, "Café lights \"flicker\".", false, {{6, 12, Style::italic}}, "e2"},
               {T::cue, "MARC", false, {}, "e3"},
               {T::dialogue, "Hello.", false, {}, "e4"},
               {T::cue, "ANNA", true, {}, "e5"},
               {T::parenthetical, "(quietly)", false, {}, "e6"},
               {T::dialogue, "Line one\nline 𝄞 two", false,
                {{0, 4, Style::bold}, {19, 22, Style::bold}, {19, 22, Style::underline}}, "e7"},
               {T::page_break, "", false, {}, "e8"},
               {T::transition, "CUT TO:", false, {}, "e9"},
               {T::lyric, "", false, {}, "e10"},
               {T::centered, "END OF ACT ONE", false, {}, "e11"}}};
}

QByteArray with_elements(const char* elements, const char* title_page = "[]") {
  return QByteArray(R"({"format": "apollo-screenplay", "version": 1, "revision": 0, "titlePage": )") + title_page +
         R"(, "elements": [)" + elements + "]}";
}

SourceError::Kind failure(const QByteArray& bytes) {
  try {
    parse_screenplay(bytes);
  } catch (const SourceError& error) {
    return error.kind();
  }
  QTest::qFail("the source was accepted", __FILE__, __LINE__);
  return SourceError::Kind::io;
}
}

class SourceTests : public QObject {
  Q_OBJECT
private slots:
  void serializes_the_example_exactly() {
    const auto expected = read_file(QStringLiteral(APOLLO_SOURCE_FIXTURE_DIR "/v1-example.screenplay.json"));
    QVERIFY(!expected.isEmpty());
    QCOMPARE(serialize_screenplay(example()), expected);
  }

  void parses_the_example() {
    const auto parsed = parse_screenplay(read_file(QStringLiteral(APOLLO_SOURCE_FIXTURE_DIR "/v1-example.screenplay.json")));
    QCOMPARE(parsed.revision, example().revision);
    QCOMPARE(parsed.elements.size(), example().elements.size());
    for (std::size_t i = 0; i < parsed.elements.size(); ++i)
      QVERIFY2(parsed.elements[i] == example().elements[i], parsed.elements[i].id.c_str());
  }

  // Equal meaning saves as equal bytes; a dual flag off a cue has no meaning.
  void saves_a_canonical_form() {
    const Screenplay messy{7, {{T::action, "abcdef", true, {{2, 4, Style::bold}, {0, 2, Style::bold}, {1, 3, Style::italic}}, "a"},
                               {T::title, "Title", false, {}, "t"},
                               {T::cue, "ANNA", true, {}, "c"}}};
    const Screenplay tidy{7, {{T::title, "Title", false, {}, "t"},
                              {T::action, "abcdef", false, {{0, 4, Style::bold}, {1, 3, Style::italic}}, "a"},
                              {T::cue, "ANNA", true, {}, "c"}}};
    QCOMPARE(serialize_screenplay(messy), serialize_screenplay(tidy));
    QVERIFY(parse_screenplay(serialize_screenplay(messy)) == tidy);
    QVERIFY(canonical(messy) == tidy);
  }

  void rejects_malformed_sources_data() {
    QTest::addColumn<QByteArray>("bytes");
    const auto element = [](const char* json) { return with_elements(json); };
    QTest::newRow("not JSON") << QByteArray("{");
    QTest::newRow("array") << QByteArray("[]");
    QTest::newRow("other format") << QByteArray(R"({"format": "fountain", "version": 1})");
    QTest::newRow("version 0") << QByteArray(R"({"format": "apollo-screenplay", "version": 0, "revision": 0, "titlePage": [], "elements": []})");
    QTest::newRow("fractional version") << QByteArray(R"({"format": "apollo-screenplay", "version": 1.5, "revision": 0, "titlePage": [], "elements": []})");
    QTest::newRow("string version") << QByteArray(R"({"format": "apollo-screenplay", "version": "1", "revision": 0, "titlePage": [], "elements": []})");
    QTest::newRow("no revision") << QByteArray(R"({"format": "apollo-screenplay", "version": 1, "titlePage": [], "elements": []})");
    QTest::newRow("negative revision") << QByteArray(R"({"format": "apollo-screenplay", "version": 1, "revision": -1, "titlePage": [], "elements": []})");
    QTest::newRow("unknown key") << QByteArray(R"({"format": "apollo-screenplay", "version": 1, "revision": 0, "titlePage": [], "elements": [], "notes": []})");
    QTest::newRow("no id") << element(R"({"type": "action", "text": "a"})");
    QTest::newRow("empty id") << element(R"({"id": "", "type": "action", "text": "a"})");
    QTest::newRow("id with space") << element(R"({"id": "a b", "type": "action", "text": "a"})");
    QTest::newRow("repeated id") << with_elements(R"({"id": "a", "type": "action", "text": "a"})", R"([{"id": "a", "field": "title", "text": "T"}])");
    QTest::newRow("unknown type") << element(R"({"id": "a", "type": "montage", "text": "a"})");
    QTest::newRow("generated type") << element(R"j({"id": "a", "type": "more", "text": "(MORE)"})j");
    QTest::newRow("title field as element") << element(R"({"id": "a", "type": "title", "text": "a"})");
    QTest::newRow("element as title field") << with_elements("", R"([{"id": "a", "field": "action", "text": "a"}])");
    QTest::newRow("title field with type") << with_elements("", R"([{"id": "a", "type": "title", "text": "a"}])");
    QTest::newRow("no text") << element(R"({"id": "a", "type": "action"})");
    QTest::newRow("text number") << element(R"({"id": "a", "type": "action", "text": 3})");
    QTest::newRow("line separator") << element(R"({"id": "a", "type": "action", "text": "a b"})");
    QTest::newRow("paragraph separator") << element(R"({"id": "a", "type": "action", "text": "a b"})");
    QTest::newRow("carriage return") << element(R"({"id": "a", "type": "action", "text": "a\rb"})");
    QTest::newRow("frame marker") << element(R"({"id": "a", "type": "action", "text": "a\ufdd0b"})");
    QTest::newRow("lone surrogate") << element(R"({"id": "a", "type": "action", "text": "a\ud800"})");
    QTest::newRow("unknown element key") << element(R"({"id": "a", "type": "action", "text": "a", "scene": 3})");
    QTest::newRow("dual dialogue") << element(R"({"id": "a", "type": "dialogue", "text": "a", "dual": true})");
    QTest::newRow("dual string") << element(R"({"id": "a", "type": "character", "text": "A", "dual": "yes"})");
    // "a𝄞" is two code points, so 3 is past its end.
    QTest::newRow("span past end") << element(R"({"id": "a", "type": "action", "text": "a𝄞", "styles": [{"start": 1, "end": 3, "style": "bold"}]})");
    QTest::newRow("empty span") << element(R"({"id": "a", "type": "action", "text": "ab", "styles": [{"start": 1, "end": 1, "style": "bold"}]})");
    QTest::newRow("negative span") << element(R"({"id": "a", "type": "action", "text": "ab", "styles": [{"start": -1, "end": 1, "style": "bold"}]})");
    QTest::newRow("fractional span") << element(R"({"id": "a", "type": "action", "text": "ab", "styles": [{"start": 0.5, "end": 1, "style": "bold"}]})");
    QTest::newRow("unknown style") << element(R"({"id": "a", "type": "action", "text": "ab", "styles": [{"start": 0, "end": 1, "style": "strike"}]})");
    QTest::newRow("unknown span key") << element(R"({"id": "a", "type": "action", "text": "ab", "styles": [{"start": 0, "end": 1, "style": "bold", "x": 1}]})");
    QTest::newRow("styles object") << element(R"({"id": "a", "type": "action", "text": "ab", "styles": {}})");
  }
  void rejects_malformed_sources() {
    QFETCH(QByteArray, bytes);
    QCOMPARE(failure(bytes), SourceError::Kind::malformed);
  }

  // A later version may add anything; this build must not claim to read it.
  void refuses_a_newer_version() {
    QCOMPARE(failure(R"({"format": "apollo-screenplay", "version": 2, "chapters": []})"), SourceError::Kind::newer_version);
  }

  // Empty elements and text the editor can hold, such as typed text in a
  // page-break element (docs/writing-build-plan.md), still save.
  void saves_unfinished_writing() {
    const Screenplay unfinished{1, {{T::cue, "", false, {}, "a"}, {T::page_break, "typed", false, {{0, 5, Style::bold}}, "b"},
                                    {T::action, "\t\x01 odd", false, {}, "c"}, {T::dialogue, "   ", false, {}, "d"}}};
    QVERIFY(parse_screenplay(serialize_screenplay(unfinished)) == unfinished);
  }

  void revision_bounds_match_the_reader() {
    auto screenplay = example();
    screenplay.revision = 9007199254740991ULL;
    const auto bytes = serialize_screenplay(screenplay);
    QCOMPARE(QJsonDocument::fromJson(bytes).object().value("revision").toInteger(), qint64(9007199254740991LL));
    QVERIFY(parse_screenplay(bytes) == screenplay);
    screenplay.revision = 9007199254740992ULL;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, serialize_screenplay(screenplay));
    screenplay.revision = std::numeric_limits<std::uint64_t>::max();
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, serialize_screenplay(screenplay));
    QCOMPARE(failure(R"({"format":"apollo-screenplay","version":1,"revision":9007199254740992,"titlePage":[],"elements":[]})"),
             SourceError::Kind::malformed);
  }

  void refuses_to_serialize_invalid_elements_data() {
    QTest::addColumn<int>("row");
    for (int row = 0; row < 7; ++row) QTest::newRow(QByteArray::number(row)) << row;
  }
  void refuses_to_serialize_invalid_elements() {
    QFETCH(int, row);
    const std::vector<Screenplay> invalid{
        {0, {{T::action, "a", false, {}, ""}}},
        {0, {{T::action, "a", false, {}, "x"}, {T::action, "b", false, {}, "x"}}},
        {0, {{T::action, "a", false, {}, "x y"}}},
        // é is two bytes: offset 2 is inside it.
        {0, {{T::action, "Café", false, {{0, 4, Style::bold}}, "x"}}},
        {0, {{T::action, "ab", false, {{0, 3, Style::bold}}, "x"}}},
        {0, {{T::more, "(MORE)", false, {}, "x"}}},
        {0, {{T::action, "a\rb", false, {}, "x"}}}};
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, serialize_screenplay(invalid[static_cast<std::size_t>(row)]));
  }

  void creates_and_opens_a_project() {
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    const auto destination = parent.filePath(QStringLiteral("My Script"));
    const auto project = create_project(destination, example());
    QCOMPARE(QDir(project.directory).canonicalPath(), QDir(destination).canonicalPath());
    QCOMPARE(QDir(parent.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{"My Script"});
    QCOMPARE(QDir(destination).entryList(QDir::Files | QDir::Hidden), (QStringList{"project.json", "screenplay.json"}));
    // project.json, read independently of Apollo's reader.
    const auto manifest = QJsonDocument::fromJson(read_file(destination + "/project.json")).object();
    QCOMPARE(manifest.value("format").toString(), QStringLiteral("apollo-project"));
    QCOMPARE(manifest.value("version").toInt(), 1);
    QCOMPARE(manifest.value("screenplay").toString(), QStringLiteral("screenplay.json"));
    QCOMPARE(manifest.value("id").toString(), project.id);
    QVERIFY(!QUuid::fromString(project.id).isNull());
    QCOMPARE(read_file(destination + "/screenplay.json"), serialize_screenplay(example()));
    const auto opened = open_project(destination);
    QCOMPARE(opened.project.id, project.id);
    QVERIFY(opened.screenplay == example());
    QVERIFY(create_project(parent.filePath("Second"), example()).id != project.id);
  }

  void never_replaces_an_existing_destination() {
    QTemporaryDir parent;
    const auto full = parent.filePath(QStringLiteral("full")), empty = parent.filePath(QStringLiteral("empty"));
    QVERIFY(QDir().mkpath(full) && QDir().mkpath(empty) && write_file(full + "/keep.txt", "mine"));
    QVERIFY_THROWS_EXCEPTION(SourceError, create_project(full, example()));
    QVERIFY_THROWS_EXCEPTION(SourceError, create_project(empty, example()));
    QVERIFY_THROWS_EXCEPTION(SourceError, create_project(parent.filePath("missing/inner"), example()));
    QCOMPARE(read_file(full + "/keep.txt"), QByteArray("mine"));
    QVERIFY(QDir(empty).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));
    // Nothing staged is left behind.
    QCOMPARE(QDir(parent.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), (QStringList{"empty", "full"}));
  }

  // Import parsing is separate; complete project creation must retain even
  // empty dependencies and original CRLF/non-ASCII bytes exactly.
  void creates_a_project_with_import_dependencies() {
    QTemporaryDir parent;
    const ProjectAssets assets{QByteArray(), QByteArray("Title: Café\r\n\r\nAction.\r\n")};
    const auto project = create_project(parent.filePath("copy"), example(), assets);
    QVERIFY(QFile::exists(project.directory + "/imported-material.json"));
    QCOMPARE(read_file(project.directory + "/imported-material.json"), QByteArray());
    QCOMPARE(read_file(project.directory + "/imports/original.fountain"), QByteArray("Title: Café\r\n\r\nAction.\r\n"));
    QVERIFY(open_project(project.directory).assets == assets);
  }

  void refuses_linked_or_unreadable_import_dependencies_data() {
    QTest::addColumn<QString>("dependency");
    for (const auto* name : {"material symlink", "imports symlink", "original symlink", "unreadable material"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }
  void refuses_linked_or_unreadable_import_dependencies() {
    QFETCH(QString, dependency);
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), example());
    const auto data = parent.filePath("external");
    QVERIFY(write_file(data, "keep"));
    if (dependency == "material symlink") QVERIFY(QFile::link(data, project.directory + "/imported-material.json"));
    if (dependency == "imports symlink") QVERIFY(QFile::link(parent.path(), project.directory + "/imports"));
    if (dependency == "original symlink") {
      QVERIFY(QDir().mkpath(project.directory + "/imports"));
      QVERIFY(QFile::link(data, project.directory + "/imports/original.fountain"));
    }
    const auto material = project.directory + "/imported-material.json";
    if (dependency == "unreadable material") {
      QVERIFY(write_file(material, "unreadable"));
      QVERIFY(QFile::setPermissions(material, {}));
    }
    auto restore = qScopeGuard([&] {
      if (dependency == "unreadable material") QFile::setPermissions(material, QFile::ReadOwner | QFile::WriteOwner);
    });
    QVERIFY_THROWS_EXCEPTION(SourceError, ProjectSession{project.directory});
    QCOMPARE(read_file(data), QByteArray("keep"));
    QVERIFY(!QFile::exists(project.directory + "/.apollo.lock"));
  }

  void rejects_invalid_projects_data() {
    QTest::addColumn<QByteArray>("manifest");
    QTest::addColumn<int>("kind");
    const auto id = QByteArray("\"id\": \"0d3c7a52-1f0e-4f43-9a3e-2f6f3cb1f7a4\"");
    const auto malformed = static_cast<int>(SourceError::Kind::malformed);
    QTest::newRow("not JSON") << QByteArray("nope") << malformed;
    QTest::newRow("other format") << QByteArray("{\"format\": \"x\", \"version\": 1, " + id + ", \"screenplay\": \"screenplay.json\"}") << malformed;
    QTest::newRow("bad id") << QByteArray(R"({"format": "apollo-project", "version": 1, "id": "abc", "screenplay": "screenplay.json"})") << malformed;
    QTest::newRow("parent path") << QByteArray("{\"format\": \"apollo-project\", \"version\": 1, " + id + ", \"screenplay\": \"../screenplay.json\"}") << malformed;
    QTest::newRow("subfolder") << QByteArray("{\"format\": \"apollo-project\", \"version\": 1, " + id + ", \"screenplay\": \"a/screenplay.json\"}") << malformed;
    QTest::newRow("unknown key") << QByteArray("{\"format\": \"apollo-project\", \"version\": 1, " + id + ", \"screenplay\": \"screenplay.json\", \"git\": 1}") << malformed;
    QTest::newRow("newer") << QByteArray(R"({"format": "apollo-project", "version": 2})") << static_cast<int>(SourceError::Kind::newer_version);
    QTest::newRow("missing screenplay") << QByteArray("{\"format\": \"apollo-project\", \"version\": 1, " + id + ", \"screenplay\": \"other.json\"}")
                                        << static_cast<int>(SourceError::Kind::io);
  }
  void rejects_invalid_projects() {
    QFETCH(QByteArray, manifest);
    QFETCH(int, kind);
    QTemporaryDir directory;
    QVERIFY(write_file(directory.filePath("project.json"), manifest));
    QVERIFY(write_file(directory.filePath("screenplay.json"), serialize_screenplay(example())));
    try {
      open_project(directory.path());
      QFAIL("the project was accepted");
    } catch (const SourceError& error) {
      QCOMPARE(static_cast<int>(error.kind()), kind);
    }
  }

  void a_missing_project_is_an_io_error() {
    QTemporaryDir directory;
    try {
      open_project(directory.filePath("absent"));
      QFAIL("the project was accepted");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::io);
    }
  }

  // Renaming a folder retains its active edit lock. Closing the old session
  // must release that lock without deleting one at a newly reused pathname.
  void editable_lock_follows_the_folder() {
    QTemporaryDir parent;
    const auto path = parent.filePath("project"), moved = parent.filePath("moved");
    create_project(path, example());
    auto first = std::make_unique<ProjectSession>(path);
    QVERIFY(QDir().rename(path, moved));
    QVERIFY_THROWS_EXCEPTION(SourceError, ProjectSession{moved});
    QVERIFY_THROWS_EXCEPTION(SourceError, first->check_destination());
    create_project(path, example());
    auto second = std::make_unique<ProjectSession>(path);
    const auto second_lock = read_file(path + "/.apollo.lock");
    first.reset();
    QVERIFY(!QFile::exists(moved + "/.apollo.lock"));
    QCOMPARE(read_file(path + "/.apollo.lock"), second_lock);
    QVERIFY_THROWS_EXCEPTION(SourceError, ProjectSession{path});
    ProjectSession moved_session(moved);
    moved_session.check_destination();
    second.reset();
    ProjectSession reopened(path);
    reopened.check_destination();
  }

  void editable_files_cannot_alias_other_projects_data() {
    QTest::addColumn<bool>("symlink");
    QTest::newRow("file symlink") << true;
    QTest::newRow("file hard link") << false;
  }
  void editable_files_cannot_alias_other_projects() {
    QFETCH(bool, symlink);
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), example());
    const auto original = parent.filePath("shared.json");
    QVERIFY(QFile::rename(project.screenplay_path(), original));
    if (symlink) QVERIFY(QFile::link(original, project.screenplay_path()));
    else QVERIFY(::link(QFile::encodeName(original).constData(), QFile::encodeName(project.screenplay_path()).constData()) == 0);
    const auto before = read_file(original);
    QVERIFY_THROWS_EXCEPTION(SourceError, ProjectSession{project.directory});
    QCOMPARE(read_file(original), before);
    QVERIFY(!QFile::exists(project.directory + "/.apollo.lock"));
  }

  // Each phase of a failed replacement leaves the earlier complete file.
  void reload_requires_explicit_acceptance_and_rejects_a_stale_candidate() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), example());
    ProjectSession session(project.directory);
    auto external = example();
    external.elements.back().text = "External replacement.";
    const auto bytes = serialize_screenplay(external);
    replace_file(project.screenplay_path(), bytes);
    const auto candidate = session.read_reload();
    QVERIFY(candidate.screenplay == canonical(external));
    QVERIFY_THROWS_EXCEPTION(SourceError, session.save(serialize_screenplay(example())));
    // A second replacement must not be silently adopted through this candidate.
    external.elements.back().text = "Changed again.";
    replace_file(project.screenplay_path(), serialize_screenplay(external));
    QVERIFY_THROWS_EXCEPTION(SourceError, session.accept_reload(candidate));
    QCOMPARE(read_file(project.screenplay_path()), serialize_screenplay(external));
    session.accept_reload(session.read_reload());
    session.save(serialize_screenplay(external));
    session.check_destination();
  }

  void a_failed_replacement_keeps_the_old_file_data() {
    QTest::addColumn<int>("phase");
    QTest::newRow("opened") << static_cast<int>(WritePhase::opened);
    QTest::newRow("written") << static_cast<int>(WritePhase::written);
  }
  void a_failed_replacement_keeps_the_old_file() {
    QFETCH(int, phase);
    QTemporaryDir directory;
    const auto path = directory.filePath("screenplay.json");
    QVERIFY(write_file(path, "old complete"));
    QVERIFY_THROWS_EXCEPTION(SourceError, replace_file(path, "new", [&](WritePhase at) {
      if (static_cast<int>(at) == phase) throw std::runtime_error("injected failure");
    }));
    QCOMPARE(read_file(path), QByteArray("old complete"));
    QCOMPARE(QDir(directory.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{"screenplay.json"});
    replace_file(path, "new complete");
    QCOMPARE(read_file(path), QByteArray("new complete"));
  }

  void replacement_reports_filesystem_errors() {
    QTemporaryDir directory;
    const auto locked = directory.filePath("locked");
    QVERIFY(QDir().mkpath(locked) && write_file(locked + "/screenplay.json", "old"));
    QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
    try {
      replace_file(locked + "/screenplay.json", "new");
      QFAIL("a read-only folder accepted the replacement");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::io);
      QVERIFY(QString::fromUtf8(error.what()).contains("screenplay.json"));
    }
    QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QCOMPARE(read_file(locked + "/screenplay.json"), QByteArray("old"));
    QVERIFY_THROWS_EXCEPTION(SourceError, replace_file(directory.filePath("gone/screenplay.json"), "new"));
  }
};

QTEST_GUILESS_MAIN(SourceTests)
#include "source_tests.moc"
