#include "source/project_creation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <unistd.h>

using namespace apollo;
using namespace apollo::source;

namespace {
bool write_file(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray read_file(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QStringList entries(const QString& path) {
  return QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
}
}

class ProjectCreationTests : public QObject {
  Q_OBJECT
private slots:

  void blank_project_is_complete_independent_and_editable() {
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    const auto first = create_blank_project(parent.path(), QStringLiteral("  My Script  "));
    QCOMPARE(first.directory, parent.filePath("My Script"));
    QCOMPARE(first.directory, QFileInfo(first.directory).canonicalFilePath());
    QVERIFY(!QUuid::fromString(first.id).isNull());
    QCOMPARE(entries(first.directory), (QStringList{"project.json", "screenplay.json"}));

    const auto manifest = QJsonDocument::fromJson(read_file(first.directory + "/project.json")).object();
    QCOMPARE(manifest.value("id").toString(), first.id);
    QCOMPARE(manifest.value("screenplay").toString(), QStringLiteral("screenplay.json"));
    const auto json = QJsonDocument::fromJson(read_file(first.screenplay_path())).object();
    QCOMPARE(json.value("revision").toInt(-1), 0);
    QVERIFY(json.value("titlePage").toArray().isEmpty());
    QCOMPARE(json.value("elements").toArray().size(), 1);

    const auto opened = open_project(first.directory);
    QCOMPARE(opened.screenplay.revision, std::uint64_t{0});
    QCOMPARE(opened.screenplay.elements.size(), std::size_t{1});
    const auto& action = opened.screenplay.elements.front();
    QCOMPARE(action.type, ElementType::action);
    QVERIFY(action.text.empty());
    QVERIFY(!action.id.empty());
    const auto second = create_blank_project(parent.path(), "Second");
    QVERIFY(first.id != second.id);
    QVERIFY(action.id != open_project(second.directory).screenplay.elements.front().id);
    {
      ProjectSession session(first.directory);
      auto changed = session.opened().screenplay;
      changed.revision = 1;
      changed.elements.front().text = "First action.";
      session.save(serialize_screenplay(changed));
    }
    const auto reopened = open_project(first.directory);
    QCOMPARE(reopened.screenplay.revision, std::uint64_t{1});
    QCOMPARE(reopened.screenplay.elements.front().text, std::string("First action."));
    QVERIFY(open_project(second.directory).screenplay.elements.front().text.empty());
    QCOMPARE(entries(parent.path()), (QStringList{"My Script", "Second"}));
  }

  void collisions_preserve_every_kind_of_destination() {
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    QVERIFY(write_file(parent.filePath("Script"), "file evidence"));
    QVERIFY(QDir().mkdir(parent.filePath("Script (2)")));
    QVERIFY(write_file(parent.filePath("Script (2)/keep"), "directory evidence"));
    QVERIFY(QDir().mkdir(parent.filePath("Script (3)")));
    const auto missing = parent.filePath("missing");
    QVERIFY(QFile::link(missing, parent.filePath("Script (4)")));
    const auto created = create_blank_project(parent.path(), "Script");
    QCOMPARE(created.directory, parent.filePath("Script (5)"));
    QCOMPARE(read_file(parent.filePath("Script")), QByteArray("file evidence"));
    QCOMPARE(read_file(parent.filePath("Script (2)/keep")), QByteArray("directory evidence"));
    QVERIFY(entries(parent.filePath("Script (3)")).isEmpty());
    QVERIFY(QFileInfo(parent.filePath("Script (4)")).isSymLink());
    QCOMPARE(QFileInfo(parent.filePath("Script (4)")).symLinkTarget(), missing);
    QCOMPARE(entries(parent.path()), (QStringList{"Script", "Script (2)", "Script (3)", "Script (4)", "Script (5)"}));
    QCOMPARE(open_project(created.directory).project.id, created.id);
  }

  void resolves_custom_parent_alias() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkdir(root.filePath("actual")));
    QVERIFY(QFile::link(root.filePath("actual"), root.filePath("alias")));
    const auto project = create_blank_project(root.filePath("alias"), QString::fromUtf8("Café 𝄞"));
    QCOMPARE(project.directory, root.filePath(QString::fromUtf8("actual/Café 𝄞")));
    QCOMPARE(open_project(project.directory).project.id, project.id);
  }

  void invalid_names_data() {
    QTest::addColumn<QString>("name");
    QTest::newRow("empty") << QString();
    QTest::newRow("whitespace") << QStringLiteral("   ");
    QTest::newRow("dot") << QStringLiteral(" . ");
    QTest::newRow("dotdot") << QStringLiteral("..");
    QTest::newRow("slash") << QStringLiteral("one/two");
    QTest::newRow("backslash") << QStringLiteral("one\\two");
    QTest::newRow("control") << QStringLiteral("one\ntwo");
    QTest::newRow("edge-control") << QStringLiteral("\tScript");
    QTest::newRow("nul") << (QStringLiteral("one") + QChar(0) + "two");
    QTest::newRow("unpaired-high") << QString(QChar(0xd800));
    QTest::newRow("unpaired-low") << QString(QChar(0xdc00));
    QTest::newRow("overlong") << QString(65536, QLatin1Char('x'));
    QTest::newRow("overlong-utf8") << QString(65536, QChar(0x00e9));
  }
  void invalid_names() {
    QFETCH(QString, name);
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    QVERIFY_THROWS_EXCEPTION(SourceError, create_blank_project(parent.path(), name));
    QVERIFY(entries(parent.path()).isEmpty());
  }

  void invalid_parents_are_not_created() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(write_file(root.filePath("file"), "keep"));
    const QStringList parents{"", "relative", root.filePath("missing"), root.filePath("file"),
                              root.path() + "/.", root.path() + "/", root.path() + "/../" + QFileInfo(root.path()).fileName(),
                              root.path() + QChar(0)};
    for (const auto& parent : parents) {
      QVERIFY_THROWS_EXCEPTION(SourceError, create_blank_project(parent, "Script"));
      QCOMPARE(entries(root.path()), QStringList{"file"});
      QCOMPARE(read_file(root.filePath("file")), QByteArray("keep"));
    }
  }

  void bounds_collision_search_without_changing_evidence() {
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    QVERIFY(write_file(parent.filePath("Script"), "keep"));
    for (int suffix = 2; suffix <= 1000; ++suffix)
      QVERIFY(write_file(parent.filePath(QStringLiteral("Script (%1)").arg(suffix)), "keep"));
    try {
      create_blank_project(parent.path(), "Script");
      QFAIL("an exhausted collision search succeeded");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::io);
      QVERIFY(QString::fromUtf8(error.what()).contains("1000"));
    }
    QCOMPARE(entries(parent.path()).size(), 1000);
    for (const auto& entry : entries(parent.path())) QCOMPARE(read_file(parent.filePath(entry)), QByteArray("keep"));
  }

  void permission_failure_preserves_evidence_and_leaves_no_stage() {
    if (::geteuid() == 0) QSKIP("root bypasses directory write permissions");
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto parent = root.filePath("locked");
    QVERIFY(QDir().mkdir(parent));
    QVERIFY(write_file(parent + "/Script", "keep"));
    const auto permissions = QFileInfo(parent).permissions();
    const auto restore = qScopeGuard([&] { QFile::setPermissions(parent, permissions); });
    QVERIFY(QFile::setPermissions(parent, QFile::ReadOwner | QFile::ExeOwner));
    try {
      create_blank_project(parent, "Script");
      QFAIL("a read-only parent accepted a project");
    } catch (const SourceError& error) {
      QCOMPARE(error.kind(), SourceError::Kind::io);
      QVERIFY(QString::fromUtf8(error.what()).contains(parent));
    }
    QCOMPARE(entries(parent), QStringList{"Script"});
    QCOMPARE(read_file(parent + "/Script"), QByteArray("keep"));
  }
};

QTEST_GUILESS_MAIN(ProjectCreationTests)
#include "project_creation_tests.moc"
