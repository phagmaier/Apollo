#include "source/fountain_intake.h"
#include "source/project_access.h"
#include "source/project_creation.h"
#include "source/recovery.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <csignal>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace apollo;
using namespace apollo::source;
namespace {
QByteArray bytes(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("test input unreadable");
  return file.readAll();
}
Screenplay placeholder() { return {0, {{ElementType::action, ""}}}; }
QStringList entries(const QString& path) {
  return QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
}
}
class FountainIntakeTests : public QObject {
  Q_OBJECT
private slots:
  void official_and_independent_samples_data() {
    QTest::addColumn<QString>("name");
    for (const auto* name : {"Boneyard", "CenteredText", "Dialogue", "DualDialogue", "ForcedElements", "Indenting",
          "MultilineAction", "Notes", "PageBreaks", "SceneHeaders", "SceneNumbers", "SectionHeaders", "Sections-Complex",
          "Simple", "Synopses", "Transitions"}) QTest::newRow(name) << QString("fountain-syntax/%1.fountain").arg(name);
    QTest::newRow("independent") << QString("import/independent.fountain");
  }
  void official_and_independent_samples() {
    QFETCH(QString, name);
    const auto path = QStringLiteral(APOLLO_FIXTURE_ROOT) + '/' + name;
    const auto original = bytes(path);
    const auto intake = read_fountain_intake(path);
    QCOMPARE(*intake.assets.original_fountain, original);
    QVERIFY(intake.summary.contains("entire source is pending"));
    QTemporaryDir parent;
    const auto published = create_named_project(parent.path(), "Retained", placeholder(), intake.assets);
    const auto reopened = open_project(published.directory);
    QVERIFY(reopened.project.import_review);
    QCOMPARE(bytes(published.directory + "/imports/original.fountain"), original);
    QCOMPARE(bytes(path), original);
    QCOMPARE(reopened.assets, intake.assets);
    const auto manifest = QJsonDocument::fromJson(bytes(published.directory + "/project.json")).object();
    QCOMPARE(manifest.value("version"), QJsonValue(2));
    QCOMPARE(manifest.value("mode"), QJsonValue("import-review"));
  }
  void bytes_and_encoding_data() {
    QTest::addColumn<QByteArray>("original");
    QTest::addColumn<QString>("view");
    QTest::newRow("empty") << QByteArray() << QString();
    QTest::newRow("bom-crlf") << QByteArray("\xef\xbb\xbf" "A\r\n\r\nB\r") << QString("A\r\n\r\nB\r");
    QTest::newRow("unicode-controls") << QByteArray("caf\xc3\xa9\0\xe2\x80\xae<tags>", 15) << QString::fromUtf8("café\\u0000\\u202e<tags>");
    QTest::newRow("invalid-utf8") << QByteArray("\xff\xc0\x80", 3) << QString("00000000  ff c0 80\n");
    QTest::newRow("utf16") << QByteArray("\xff\xfe" "A\0", 4) << QString("00000000  ff fe 41 00\n");
  }
  void bytes_and_encoding() {
    QFETCH(QByteArray, original);
    QFETCH(QString, view);
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, original);
    const auto intake = read_fountain_intake(input);
    QCOMPARE(*intake.assets.original_fountain, original);
    QCOMPARE(intake.source_view, view);
    const auto published = create_project(parent.filePath("project"), placeholder(), intake.assets);
    QCOMPARE(*open_project(published.directory).assets.original_fountain, original);
    QCOMPARE(bytes(input), original);
  }
  void located_candidates_do_not_claim_conversion() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, "A\r\n[[note]]\r\n/*keep*/\r\n[[unclosed");
    const auto intake = read_fountain_intake(input);
    QVERIFY(intake.findings.size() >= 3);
    const auto note = intake.findings[0].toMap();
    QCOMPARE(note.value("firstLine").toInt(), 2);
    QCOMPARE(note.value("lastLine").toInt(), 2);
    QCOMPARE(note.value("startByte").toInt(), 3);
    QCOMPARE(note.value("endByte").toInt(), 11);
    QCOMPARE(note.value("text").toString(), QString("[[note]]"));
    QCOMPARE(intake.findings[1].toMap().value("firstLine").toInt(), 3);
    QVERIFY(intake.findings[2].toMap().value("message").toString().contains("Unclosed"));
    QVERIFY(intake.summary.contains("not yet been converted"));
  }
  void nonregular_missing_unreadable_and_oversize_input() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, "Keep original");
    const auto link = parent.filePath("alias");
    QVERIFY(QFile::link(input, link));
    const auto fifo = parent.filePath("fifo");
    QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
    for (const auto& path : {parent.filePath("missing"), parent.path(), link, fifo})
      QVERIFY_THROWS_EXCEPTION(SourceError, read_fountain_intake(path));
    if (::geteuid() != 0) {
      QVERIFY(QFile::setPermissions(input, {}));
      const auto restore = qScopeGuard([&] { QFile::setPermissions(input, QFile::ReadOwner | QFile::WriteOwner); });
      QVERIFY_THROWS_EXCEPTION(SourceError, read_fountain_intake(input));
    }
    const auto huge = parent.filePath("huge");
    QFile file(huge);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(maximum_fountain_bytes + 1));
    file.close();
    QVERIFY_THROWS_EXCEPTION(SourceError, read_fountain_intake(huge));
    QVERIFY(QFileInfo(link).isSymLink());
  }
  void publication_failure_rolls_back_stage_and_preserves_input() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, QByteArray(4096, 'x'));
    const auto intake = read_fountain_intake(input);
    struct rlimit previous {};
    QCOMPARE(::getrlimit(RLIMIT_FSIZE, &previous), 0);
    const auto handler = std::signal(SIGXFSZ, SIG_IGN);
    const auto restore = qScopeGuard([&] { ::setrlimit(RLIMIT_FSIZE, &previous); std::signal(SIGXFSZ, handler); });
    auto limited = previous;
    limited.rlim_cur = 1024; // JSON fits, the retained original cannot finish.
    QCOMPARE(::setrlimit(RLIMIT_FSIZE, &limited), 0);
    QVERIFY_THROWS_EXCEPTION(SourceError, create_named_project(parent.path(), "Review", placeholder(), intake.assets));
    QCOMPARE(entries(parent.path()), QStringList{"input"});
    QCOMPARE(bytes(input), QByteArray(4096, 'x'));
  }
  void bounded_findings_and_excerpts_keep_the_complete_unicode_source() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    // UTF-8 é straddles the 2000-byte excerpt boundary after [[ + 1997 a's.
    const auto original = QByteArray("[[") + QByteArray(1997, 'a') + "\xc3\xa9]]\n";
    replace_file(input, original);
    const auto intake = read_fountain_intake(input);
    const auto finding = intake.findings.front().toMap();
    QVERIFY(finding.value("truncated").toBool());
    QVERIFY(!finding.value("text").toString().contains(QChar(0xfffd)));
    QVERIFY(intake.source_view.contains(QString::fromUtf8("é")));
    QCOMPARE(*intake.assets.original_fountain, original);
    QByteArray many;
    for (int i = 0; i < 201; ++i) many += "[[note]]\n";
    many += "Last retained writing.";
    replace_file(input, many);
    const auto bounded = read_fountain_intake(input);
    QCOMPARE(bounded.findings.size(), 200);
    QVERIFY(bounded.summary.contains("capped at 200"));
    QVERIFY(bounded.source_view.endsWith("Last retained writing."));
    QCOMPARE(*bounded.assets.original_fountain, many);
  }
  void malformed_newer_missing_and_changed_assets_are_refused() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, "[[Retain me]]\n");
    const auto intake = read_fountain_intake(input);
    for (const int mutation : {0, 1, 2, 3, 4, 5, 6}) {
      auto assets = intake.assets;
      auto metadata = QJsonDocument::fromJson(*assets.imported_material).object();
      if (mutation == 0) assets.original_fountain.reset();
      if (mutation == 1) *assets.original_fountain += 'x';
      if (mutation == 2) metadata.insert("version", 2);
      if (mutation == 3) metadata.insert("future", true);
      if (mutation == 4) metadata.insert("status", "converted");
      if (mutation == 5) metadata.insert("bytes", -1);
      if (mutation == 6) (*assets.original_fountain)[0] = 'x'; // Same length, different hash.
      assets.imported_material = QJsonDocument(metadata).toJson();
      QVERIFY_THROWS_EXCEPTION(SourceError, open_fountain_intake(assets));
    }
    const auto project = create_project(parent.filePath("review"), placeholder(), intake.assets);
    const auto source = bytes(project.screenplay_path());
    replace_file(project.directory + "/imports/original.fountain", "changed bytes");
    ProjectAccess refused(project.directory);
    QVERIFY(!refused.current_available());
    QVERIFY(!refused.error().isEmpty());
    QCOMPARE(bytes(project.directory + "/imports/original.fountain"), QByteArray("changed bytes"));
    QCOMPARE(bytes(project.screenplay_path()), source);
  }
  void manifest_cannot_downgrade_intake_or_hide_authored_writing() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, "Original.");
    const auto intake = read_fountain_intake(input);
    const auto project = create_project(parent.filePath("review"), placeholder(), intake.assets);
    const auto manifest_path = project.directory + "/project.json";
    const auto original_manifest = bytes(manifest_path);
    {
      ProjectSession session(project.directory);
      const auto saved = bytes(project.screenplay_path());
      auto attempted = session.opened().screenplay;
      attempted.revision = 1;
      attempted.elements.front().text = "Must not replace source";
      QVERIFY_THROWS_EXCEPTION(SourceError, session.save(serialize_screenplay(attempted)));
      QCOMPARE(bytes(project.screenplay_path()), saved);
    }
    auto manifest = QJsonDocument::fromJson(original_manifest).object();
    manifest.remove("mode"); manifest.insert("version", 1);
    replace_file(manifest_path, QJsonDocument(manifest).toJson());
    QVERIFY_THROWS_EXCEPTION(SourceError, open_project(project.directory));
    replace_file(manifest_path, original_manifest);
    auto hidden = open_project(project.directory).screenplay;
    hidden.revision = 1;
    hidden.elements.front().text = "Hidden writing";
    replace_file(project.screenplay_path(), serialize_screenplay(hidden));
    QVERIFY_THROWS_EXCEPTION(SourceError, open_project(project.directory));
    QVERIFY_THROWS_EXCEPTION(SourceError, create_project(parent.filePath("invalid"), {1, {{ElementType::action, "Hidden writing"}}}, intake.assets));
    QVERIFY(!QFileInfo::exists(parent.filePath("invalid")));
  }
  void collision_copy_and_recovery_keep_review_mode_and_exact_material() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, "# Organization\n\n[[note]]\n/*omitted*/");
    const auto intake = read_fountain_intake(input);
    replace_file(parent.filePath("Review"), "collision evidence");
    const auto project = create_named_project(parent.path(), "Review", placeholder(), intake.assets);
    QCOMPARE(project.directory, parent.filePath("Review (2)"));
    QCOMPARE(bytes(parent.filePath("Review")), QByteArray("collision evidence"));
    const auto opened = open_project(project.directory);
    const auto copy = create_project(parent.filePath("copy"), opened.screenplay, opened.assets);
    QVERIFY(copy.import_review);
    QVERIFY(copy.id != project.id);
    RecoveryStore store(project, opened.assets, project.directory + "/.recovery");
    const auto package = store.preserve(opened.screenplay, "reload", QDateTime::fromString("2026-10-28T12:00:00.000Z", Qt::ISODateWithMs));
    QVERIFY(RecoveryStore::inspect_copy(package.directory)->opened.project.import_review);
    replace_file(project.directory + "/imported-material.json", "damaged evidence");
    ProjectAccess access(project.directory);
    QVERIFY(!access.current_available());
    QCOMPARE(access.candidates().size(), std::size_t(1));
    const auto recovered = access.recover(0, parent.filePath("recovered"));
    QVERIFY(recovered->opened().project.import_review);
    QCOMPARE(recovered->opened().assets, intake.assets);
    QCOMPARE(bytes(project.directory + "/imported-material.json"), QByteArray("damaged evidence"));
    QCOMPARE(bytes(package.directory + "/imports/original.fountain"), *intake.assets.original_fountain);
  }
  void reload_cannot_adopt_a_review_only_mode_into_an_existing_writer() {
    QTemporaryDir parent;
    const auto input = parent.filePath("input");
    replace_file(input, "All of this source remains pending conversion.");
    const auto intake = read_fountain_intake(input);
    const auto writer = create_project(parent.filePath("writer"), {1, {{ElementType::action, "Current writing"}}});
    ProjectSession session(writer.directory);
    const auto review = create_project(parent.filePath("review"), placeholder(), intake.assets);
    auto manifest = QJsonDocument::fromJson(bytes(review.directory + "/project.json")).object();
    manifest.insert("id", writer.id);
    replace_file(writer.directory + "/project.json", QJsonDocument(manifest).toJson());
    replace_file(writer.screenplay_path(), bytes(review.screenplay_path()));
    replace_file(writer.directory + "/imported-material.json", *intake.assets.imported_material);
    QVERIFY(QDir().mkdir(writer.directory + "/imports"));
    replace_file(writer.directory + "/imports/original.fountain", *intake.assets.original_fountain);
    QVERIFY_THROWS_EXCEPTION(SourceError, session.read_reload());
    QVERIFY(!session.opened().project.import_review);
    QCOMPARE(session.opened().screenplay.elements.front().text, std::string("Current writing"));
    QCOMPARE(bytes(writer.directory + "/imports/original.fountain"), *intake.assets.original_fountain);
  }
};
QTEST_GUILESS_MAIN(FountainIntakeTests)
#include "fountain_intake_tests.moc"
