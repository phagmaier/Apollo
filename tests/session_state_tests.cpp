#include "editor/session_state.h"
#include "editor/screenplay_layout.h"
#include "source/project.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QtTest>
#include <limits>

using namespace apollo;
using namespace apollo::editor;
using namespace apollo::source;

namespace {
const QString project_id = QStringLiteral("0d3c7a52-1f0e-4f43-9a3e-2f6f3cb1f7a4");

QJsonObject valid_json() {
  return {{"format", "apollo-session"}, {"version", 1}, {"directory", "/tmp/screenplay"},
          {"projectId", project_id}, {"elementId", "second"}, {"offset", 4}, {"position", 10}};
}

QByteArray read_file(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return file.readAll();
}

void populate(QTextDocument& document, const std::vector<Element>& elements) {
  QTextCursor cursor(&document);
  insert_elements(cursor, elements);
}
}

class SessionStateTests : public QObject {
  Q_OBJECT
private slots:
  void default_state_lives_outside_the_project() {
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    const auto data = parent.filePath("app-data");
    const bool had_data = qEnvironmentVariableIsSet("XDG_DATA_HOME");
    const auto previous_data = qgetenv("XDG_DATA_HOME");
    QVERIFY(qputenv("XDG_DATA_HOME", data.toUtf8()));
    const auto restore = qScopeGuard([&] {
      if (had_data) qputenv("XDG_DATA_HOME", previous_data);
      else qunsetenv("XDG_DATA_HOME");
    });
    QCOMPARE(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation), data);
    const auto project = create_project(parent.filePath("project"), {0, {{ElementType::action, "Before."}}});
    const auto entries = QDir(project.directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    const auto source = read_file(project.screenplay_path());
    const auto manifest = read_file(project.directory + "/project.json");
    SessionState state;
    QVERIFY(!state.load());
    state.save({project.directory, project.id, "first", 7, 12});
    const auto path = data + "/Apollo/session.json";
    QVERIFY(QFile::exists(path));
    const auto resumed = state.load();
    QVERIFY(resumed);
    QCOMPARE(resumed->directory, project.directory);
    QCOMPARE(resumed->project_id, project.id);
    QCOMPARE(resumed->element_id, QStringLiteral("first"));
    QCOMPARE(resumed->offset, 7);
    QCOMPARE(resumed->position, 12);
    QCOMPARE(QDir(project.directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), entries);
    QCOMPARE(read_file(project.screenplay_path()), source);
    QCOMPARE(read_file(project.directory + "/project.json"), manifest);
  }

  void explicit_path_missing_and_replacement() {
    QTemporaryDir parent;
    const auto path = parent.filePath("state/nested/session.json");
    SessionState state(path);
    QVERIFY(!state.load());
    state.save({parent.filePath("unavailable-project"), project_id, "", 0, 0});
    state.save({parent.filePath("other-project"), project_id, "element_2", 19, 37});
    const auto resumed = SessionState(path).load();
    QVERIFY(resumed);
    QCOMPARE(resumed->directory, parent.filePath("other-project"));
    QCOMPARE(resumed->offset, 19);
    QCOMPARE(resumed->position, 37);
    QCOMPARE(resumed->element_id, QStringLiteral("element_2"));
  }

  void stable_identity_survives_reorder_and_shortening() {
    QTextDocument original;
    populate(original, {{ElementType::action, "One.", false, {}, "first"},
                        {ElementType::action, "Second block.", false, {}, "second"}});
    // "One." occupies positions 0..3; separator 4; second block starts at 5.
    const auto captured = SessionState::capture("/tmp/project", project_id, original, 12);
    QCOMPARE(captured.element_id, QStringLiteral("second"));
    QCOMPARE(captured.offset, 7);
    QCOMPARE(captured.position, 12);
    QTextDocument reordered;
    populate(reordered, {{ElementType::action, "Second block.", false, {}, "second"},
                         {ElementType::action, "One.", false, {}, "first"}});
    QCOMPARE(SessionState::cursor_position(captured, reordered), 7);
    QTextDocument shortened;
    populate(shortened, {{ElementType::action, "One.", false, {}, "first"},
                         {ElementType::action, "Hi", false, {}, "second"}});
    QCOMPARE(SessionState::cursor_position(captured, shortened), 7);
  }

  void deleted_identity_uses_bounded_absolute_position() {
    const ResumeState state{"/tmp/project", project_id, "deleted", 100, 8};
    QTextDocument document;
    populate(document, {{ElementType::action, "abcd", false, {}, "first"},
                        {ElementType::action, "efghi", false, {}, "second"}});
    QCOMPARE(SessionState::cursor_position(state, document), 8);
    auto past_end = state;
    past_end.position = std::numeric_limits<int>::max();
    QCOMPARE(SessionState::cursor_position(past_end, document), 10);
    past_end.position = -1;
    QCOMPARE(SessionState::cursor_position(past_end, document), 0);
    QTextDocument empty;
    QCOMPARE(SessionState::cursor_position(state, empty), 0);
  }

  void unmapped_documents_capture_a_bounded_fallback() {
    QTextDocument document;
    document.setPlainText(QStringLiteral("First\nSecond"));
    const auto state = SessionState::capture("/tmp/project", project_id, document, 9);
    QVERIFY(state.element_id.isEmpty());
    QCOMPARE(state.offset, 3);
    QCOMPARE(state.position, 9);
    QCOMPARE(SessionState::cursor_position(state, document), 9);
    const auto before_start = SessionState::capture("/tmp/project", project_id, document, -9);
    QCOMPARE(before_start.offset, 0);
    QCOMPARE(before_start.position, 0);
    const auto separator = SessionState::capture("/tmp/project", project_id, document, 5);
    QCOMPARE(separator.offset, 5);
    QCOMPARE(separator.position, 5);
  }

  void utf16_mapping_never_splits_surrogates() {
    QTextDocument document;
    populate(document, {{ElementType::action, "Intro", false, {}, "first"},
                        {ElementType::action, "A🚀é\nZ", false, {}, "unicode"}});
    // Second block starts at 6: A=6, rocket=7..8, é=9, break=10, Z=11.
    const auto after_rocket = SessionState::capture("/tmp/project", project_id, document, 9);
    QCOMPARE(after_rocket.element_id, QStringLiteral("unicode"));
    QCOMPARE(after_rocket.offset, 3);
    QCOMPARE(after_rocket.position, 9);
    const auto at_break = SessionState::capture("/tmp/project", project_id, document, 11);
    QCOMPARE(at_break.offset, 5);
    const auto inside_pair = SessionState::capture("/tmp/project", project_id, document, 8);
    QCOMPARE(inside_pair.offset, 1);
    QCOMPARE(inside_pair.position, 7);
    auto state = after_rocket;
    state.offset = 2;
    QCOMPARE(SessionState::cursor_position(state, document), 7);
    state.element_id = "missing";
    state.position = 8;
    QCOMPARE(SessionState::cursor_position(state, document), 7);
    const auto past_end = SessionState::capture("/tmp/project", project_id, document, 1000);
    QCOMPARE(past_end.offset, 6);
    QCOMPARE(past_end.position, 12);
  }

  void rejects_and_preserves_invalid_state_data() {
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<int>("kind");
    const int malformed = static_cast<int>(SourceError::Kind::malformed);
    QTest::newRow("not-json") << QByteArray("not JSON") << malformed;
    QTest::newRow("array") << QByteArray("[]") << malformed;
    QTest::newRow("newer") << QByteArray(R"({"version":2,"future":true})") << static_cast<int>(SourceError::Kind::newer_version);
    const auto row = [&](const char* name, const char* key, const QJsonValue& value) {
      auto json = valid_json();
      json.insert(key, value);
      QTest::newRow(name) << QJsonDocument(json).toJson() << malformed;
    };
    row("wrong-format", "format", "other");
    row("string-version", "version", "1");
    row("unknown-key", "future", true);
    row("relative-directory", "directory", "script");
    row("unclean-directory", "directory", "/tmp/../script");
    row("nul-directory", "directory", QString("/tmp/") + QChar::Null + "script");
    row("invalid-uuid", "projectId", "not-a-uuid");
    row("uppercase-uuid", "projectId", project_id.toUpper());
    row("invalid-element-id", "elementId", "contains space");
    row("nonstring-element-id", "elementId", 42);
    row("negative-offset", "offset", -1);
    row("fractional-offset", "offset", 1.5);
    row("boolean-position", "position", true);
    row("overflow-position", "position", 2147483648.0);
    auto missing = valid_json();
    missing.remove("elementId");
    QTest::newRow("missing-key") << QJsonDocument(missing).toJson() << malformed;
  }

  void rejects_and_preserves_invalid_state() {
    QFETCH(QByteArray, bytes);
    QFETCH(int, kind);
    QTemporaryDir parent;
    const auto path = parent.filePath("session.json");
    replace_file(path, bytes);
    SessionState state(path);
    try {
      state.load();
      QFAIL("invalid session state was accepted");
    } catch (const SourceError& error) {
      QCOMPARE(static_cast<int>(error.kind()), kind);
      QVERIFY(QString::fromUtf8(error.what()).contains(path));
    }
    QVERIFY_THROWS_EXCEPTION(SourceError, state.save({parent.path(), project_id, "first", 0, 0}));
    QCOMPARE(read_file(path), bytes);
  }

  void invalid_save_keeps_previous_state() {
    QTemporaryDir parent;
    const auto path = parent.filePath("session.json");
    SessionState state(path);
    ResumeState valid{parent.path(), project_id, "first", 3, 8};
    state.save(valid);
    const auto before = read_file(path);
    auto invalid = valid;
    invalid.offset = -1;
    QVERIFY_THROWS_EXCEPTION(SourceError, state.save(invalid));
    QCOMPARE(read_file(path), before);
  }

  void io_failure_preserves_previous_state() {
    QTemporaryDir parent;
    const auto folder = parent.filePath("state");
    const auto path = folder + "/session.json";
    SessionState state(path);
    state.save({parent.path(), project_id, "first", 3, 8});
    const auto before = read_file(path);
    QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::ExeOwner));
    const auto restore = qScopeGuard([&] { QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    try {
      state.save({parent.path(), project_id, "second", 6, 15});
      QFAIL("write to read-only state folder succeeded");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::io);
      QVERIFY(QString::fromUtf8(error.what()).contains(path));
    }
    QCOMPARE(read_file(path), before);
  }

  void refuses_nonregular_state_paths() {
    QTemporaryDir parent;
    const auto target = parent.filePath("original.json");
    const auto bytes = QJsonDocument(valid_json()).toJson();
    replace_file(target, bytes);
    const auto link = parent.filePath("session.json");
    QVERIFY(QFile::link(target, link));
    SessionState state(link);
    QVERIFY_THROWS_EXCEPTION(SourceError, state.load());
    QVERIFY_THROWS_EXCEPTION(SourceError, state.save({parent.path(), project_id, "first", 0, 0}));
    QCOMPARE(read_file(target), bytes);
    QVERIFY(QFileInfo(link).isSymLink());
    SessionState directory(parent.path());
    QVERIFY_THROWS_EXCEPTION(SourceError, directory.load());
  }
};
QTEST_MAIN(SessionStateTests)
#include "session_state_tests.moc"
