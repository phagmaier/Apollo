#include "editor/project_library.h"
#include "source/project.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <csignal>
#include <sys/resource.h>
#include <sys/stat.h>

using namespace apollo::editor;
using namespace apollo::source;

namespace {
const QString first_id = QStringLiteral("0d3c7a52-1f0e-4f43-9a3e-2f6f3cb1f7a4");
const QString second_id = QStringLiteral("8bd52d47-644d-461d-bf40-a848554f49ad");
const QString third_id = QStringLiteral("db2d88ec-1ecf-4978-9dc5-6654affaa819");

QByteArray read_file(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return file.readAll();
}

QJsonObject valid_json() {
  return {{"format", "apollo-recent-projects"}, {"version", 1},
          {"projects", QJsonArray{QJsonObject{{"directory", "/missing/screenplay"}, {"projectId", first_id}}}}};
}
}

class ProjectLibraryTests : public QObject {
  Q_OBJECT
private slots:
  void default_list_is_app_local_without_project_writes() {
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
    const auto project = create_project(parent.filePath("project"), {0, {{apollo::ElementType::action, "Before."}}});
    const auto entries = QDir(project.directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    const auto manifest = read_file(project.directory + "/project.json");
    const auto screenplay = read_file(project.screenplay_path());
    // Holding the screenplay lock does not prevent app-local list access.
    ProjectSession session(project.directory);
    ProjectLibrary library;
    QVERIFY(library.load().empty());
    QVERIFY(!QFileInfo::exists(data));
    library.record({project.directory, project.id});
    QVERIFY(QFileInfo::exists(data + "/Apollo/recent-projects.json"));
    QCOMPARE(library.load().size(), std::size_t(1));
    QCOMPARE(library.load().front().project_id, project.id);
    const auto during = QDir(project.directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    auto expected = entries;
    expected.append(".apollo.lock");
    expected.sort();
    QCOMPARE(during, expected);
    QCOMPARE(read_file(project.directory + "/project.json"), manifest);
    QCOMPARE(read_file(project.screenplay_path()), screenplay);
  }

  void promotion_merges_other_instances_and_retains_missing_projects() {
    QTemporaryDir parent;
    const auto path = parent.filePath("nested/recent-projects.json");
    ProjectLibrary first(path), second(path);
    QVERIFY(first.load().empty());
    QVERIFY(second.load().empty());
    const auto missing = parent.filePath("missing");
    first.record({missing, first_id});
    second.record({parent.filePath("also-missing"), second_id});
    auto projects = first.load();
    QCOMPARE(projects.size(), std::size_t(2));
    QCOMPARE(projects[0].project_id, second_id);
    QCOMPARE(projects[1].directory, missing);
    first.record({missing, first_id});
    projects = second.load();
    QCOMPARE(projects.size(), std::size_t(2));
    QCOMPARE(projects[0].project_id, first_id);
    QCOMPARE(projects[1].project_id, second_id);
    QVERIFY(!QFileInfo::exists(missing));
  }

  void canonicalizes_aliases_moves_and_replaced_identities() {
    QTemporaryDir parent;
    const auto original = parent.filePath("original");
    const auto moved = parent.filePath("moved");
    const auto alias = parent.filePath("alias");
    QVERIFY(QDir().mkdir(original));
    QVERIFY(QFile::link(original, alias));
    ProjectLibrary library(parent.filePath("recents.json"));
    library.record({alias + "/../alias/.", "{" + first_id.toUpper() + "}"});
    auto projects = library.load();
    QCOMPARE(projects.size(), std::size_t(1));
    QCOMPARE(projects[0].directory, QFileInfo(original).canonicalFilePath());
    QCOMPARE(projects[0].project_id, first_id);
    library.record({original, first_id});
    QCOMPARE(library.load().size(), std::size_t(1));
    QVERIFY(QDir().rename(original, moved));
    QCOMPARE(library.load()[0].directory, original); // Missing paths remain visible.
    library.record({moved, first_id});
    QCOMPARE(library.load().size(), std::size_t(1));
    QCOMPARE(library.load()[0].directory, QFileInfo(moved).canonicalFilePath());
    library.record({parent.filePath("other"), second_id});
    // A successful open sees a different actual identity at the same path.
    library.record({moved, third_id});
    projects = library.load();
    QCOMPARE(projects.size(), std::size_t(2));
    QCOMPARE(projects[0].project_id, third_id);
    QCOMPARE(projects[1].project_id, second_id);
  }

  void caps_twenty_most_recent_activations() {
    QTemporaryDir parent;
    ProjectLibrary library(parent.filePath("recents.json"));
    for (int i = 0; i < 25; ++i) {
      const auto id = QStringLiteral("00000000-0000-4000-8000-%1").arg(i + 1, 12, 10, QLatin1Char('0'));
      library.record({parent.filePath(QString::number(i)), id});
    }
    const auto projects = library.load();
    QCOMPARE(projects.size(), std::size_t(20));
    for (int i = 0; i < 20; ++i)
      QCOMPARE(projects[i].directory, parent.filePath(QString::number(24 - i)));
    const auto root = QJsonDocument::fromJson(read_file(parent.filePath("recents.json"))).object();
    QCOMPARE(root.value("projects").toArray().size(), 20);
  }

  void malformed_evidence_is_preserved_data() {
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
    row("unexpected-field", "future", true);
    row("nonarray-projects", "projects", true);
    const auto entry = [&](const char* name, const QJsonObject& project) {
      row(name, "projects", QJsonArray{project});
    };
    entry("relative-path", {{"directory", "screenplay"}, {"projectId", first_id}});
    entry("unclean-path", {{"directory", "/tmp/../screenplay"}, {"projectId", first_id}});
    entry("nul-path", {{"directory", QString("/tmp/") + QChar::Null}, {"projectId", first_id}});
    entry("invalid-id", {{"directory", "/missing/screenplay"}, {"projectId", "not-a-uuid"}});
    entry("uppercase-id", {{"directory", "/missing/screenplay"}, {"projectId", first_id.toUpper()}});
    entry("missing-id", {{"directory", "/missing/screenplay"}});
    entry("unknown-entry-field", {{"directory", "/missing/screenplay"}, {"projectId", first_id}, {"extra", true}});
    QJsonArray oversized;
    for (int i = 0; i < 21; ++i) oversized.append(valid_json().value("projects").toArray().first());
    row("oversized-list", "projects", oversized);
  }

  void malformed_evidence_is_preserved() {
    QFETCH(QByteArray, bytes);
    QFETCH(int, kind);
    QTemporaryDir parent;
    const auto path = parent.filePath("recents.json");
    replace_file(path, bytes);
    ProjectLibrary library(path);
    for (const bool recording : {false, true}) {
      try {
        if (recording) library.record({parent.filePath("project"), second_id});
        else library.load();
        QFAIL("invalid recents were accepted");
      } catch (const SourceError& error) {
        QCOMPARE(static_cast<int>(error.kind()), kind);
        QVERIFY(QString::fromUtf8(error.what()).contains(path));
      }
      QCOMPARE(read_file(path), bytes);
    }
  }

  void nonregular_paths_are_not_followed_or_replaced() {
    QTemporaryDir parent;
    const auto target = parent.filePath("original.json");
    const auto bytes = QJsonDocument(valid_json()).toJson();
    replace_file(target, bytes);
    const auto link = parent.filePath("recents.json");
    QVERIFY(QFile::link(target, link));
    ProjectLibrary library(link);
    QVERIFY_THROWS_EXCEPTION(SourceError, library.load());
    QVERIFY_THROWS_EXCEPTION(SourceError, library.record({parent.path(), first_id}));
    QCOMPARE(read_file(target), bytes);
    QVERIFY(QFileInfo(link).isSymLink());
    QVERIFY(QFile::remove(target));
    QVERIFY_THROWS_EXCEPTION(SourceError, library.record({parent.path(), first_id}));
    QVERIFY(QFileInfo(link).isSymLink());
    ProjectLibrary directory(parent.path());
    QVERIFY_THROWS_EXCEPTION(SourceError, directory.load());
    QVERIFY_THROWS_EXCEPTION(SourceError, directory.record({parent.path(), first_id}));
    const auto fifo = parent.filePath("fifo");
    QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
    ProjectLibrary pipe(fifo);
    QVERIFY_THROWS_EXCEPTION(SourceError, pipe.load());
    QVERIFY_THROWS_EXCEPTION(SourceError, pipe.record({parent.path(), first_id}));
    struct stat status {};
    QCOMPARE(::lstat(QFile::encodeName(fifo).constData(), &status), 0);
    QVERIFY(S_ISFIFO(status.st_mode));
  }

  void busy_lock_fails_immediately_then_merges_fresh_state() {
    QTemporaryDir parent;
    const auto path = parent.filePath("recents.json");
    ProjectLibrary first(path), second(path);
    first.record({parent.filePath("first"), first_id});
    QLockFile lock(path + ".lock");
    lock.setStaleLockTime(0);
    QVERIFY(lock.tryLock(0));
    const auto before = read_file(path);
    QElapsedTimer timer;
    timer.start();
    try {
      second.record({parent.filePath("second"), second_id});
      QFAIL("busy list was written without owning its lock");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::locked);
      QVERIFY(QString::fromUtf8(error.what()).contains(path));
    }
    QVERIFY(timer.elapsed() < 1000);
    QCOMPARE(read_file(path), before);
    QCOMPARE(second.load().size(), std::size_t(1));
    lock.unlock();
    first.record({parent.filePath("third"), third_id});
    second.record({parent.filePath("second"), second_id});
    const auto projects = first.load();
    QCOMPARE(projects.size(), std::size_t(3));
    QCOMPARE(projects[0].project_id, second_id);
    QCOMPARE(projects[1].project_id, third_id);
    QCOMPARE(projects[2].project_id, first_id);
    QVERIFY(!QFileInfo::exists(path + ".lock"));
  }

  void failed_atomic_replacement_preserves_previous_bytes() {
    QTemporaryDir parent;
    const auto path = parent.filePath("recents.json");
    ProjectLibrary library(path);
    for (int i = 0; i < 12; ++i) {
      const auto id = QStringLiteral("00000000-0000-4000-8000-%1").arg(i + 1, 12, 10, QLatin1Char('0'));
      library.record({parent.filePath(QString::number(i)), id});
    }
    const auto before = read_file(path);
    QVERIFY(before.size() > 1024);
    struct rlimit previous {};
    QCOMPARE(::getrlimit(RLIMIT_FSIZE, &previous), 0);
    const auto previous_handler = std::signal(SIGXFSZ, SIG_IGN);
    QVERIFY(previous_handler != SIG_ERR);
    const auto restore = qScopeGuard([&] {
      ::setrlimit(RLIMIT_FSIZE, &previous);
      std::signal(SIGXFSZ, previous_handler);
    });
    auto limited = previous;
    limited.rlim_cur = 1024; // The small lock fits, but the JSON replacement does not.
    QCOMPARE(::setrlimit(RLIMIT_FSIZE, &limited), 0);
    try {
      library.record({parent.filePath("new-project"), second_id});
      QFAIL("replacement exceeded the actual filesystem write limit");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::io);
      QVERIFY(QString::fromUtf8(error.what()).contains(path));
    }
    QCOMPARE(read_file(path), before);
    QVERIFY(!QFileInfo::exists(path + ".lock"));
    QCOMPARE(library.load().size(), std::size_t(12));
  }

  void invalid_record_and_real_write_failure_preserve_list() {
    QTemporaryDir parent;
    const auto folder = parent.filePath("app-data");
    const auto path = folder + "/recents.json";
    ProjectLibrary library(path);
    library.record({parent.path(), first_id});
    const auto before = read_file(path);
    QVERIFY_THROWS_EXCEPTION(SourceError, library.record({{}, first_id}));
    QVERIFY_THROWS_EXCEPTION(SourceError, library.record({parent.path(), "not-a-uuid"}));
    QCOMPARE(read_file(path), before);
    QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::ExeOwner));
    const auto restore = qScopeGuard([&] { QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    try {
      library.record({parent.filePath("other"), second_id});
      QFAIL("record in a read-only app-data folder succeeded");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::io);
      QVERIFY(QString::fromUtf8(error.what()).contains(path));
    }
    QCOMPARE(read_file(path), before);
  }
};

QTEST_GUILESS_MAIN(ProjectLibraryTests)
#include "project_library_tests.moc"
