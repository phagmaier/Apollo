#include "source/recovery.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QtTest>
#include <algorithm>

using namespace apollo;
using namespace apollo::source;

class RecoveryTests : public QObject {
  Q_OBJECT
private slots:
  void complete_packages_and_corrupt_evidence() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("original"), {1, {{ElementType::action, "Before."}}});
    const ProjectAssets assets{QByteArray("notes\r\n"), QByteArray("Title: Original\r\n")};
    RecoveryStore store(project, assets, parent.filePath("recovery"));
    QVERIFY(QDir().rename(project.directory, project.directory + "-moved"));
    const auto time = QDateTime::fromString("2026-10-28T12:00:00.000Z", Qt::ISODateWithMs);
    const auto saved = store.preserve({7, {{ElementType::action, "Latest writing.", false, {}, "0123456789abcdef"}}}, "emergency", time);
    QVERIFY(QFile::exists(saved.directory + "/project.json"));
    const auto opened = open_project(saved.directory);
    QCOMPARE(opened.screenplay.revision, 7u);
    QCOMPARE(opened.screenplay.elements.front().text, std::string("Latest writing."));
    QVERIFY(opened.assets == assets);
    QCOMPARE(store.verified().size(), 1u);
    QCOMPARE(store.verified().front().captured_at, time);
    QFile metadata(saved.directory + "/recovery.json");
    QVERIFY(metadata.open(QIODevice::ReadOnly));
    const auto json = QJsonDocument::fromJson(metadata.readAll()).object();
    QCOMPARE(json.value("originId").toString(), project.id);
    QCOMPARE(json.value("originDirectory").toString(), project.directory);
    replace_file(saved.directory + "/screenplay.json", serialize_screenplay({8, {{ElementType::action, "Tampered.", false, {}, "0123456789abcdef"}}}));
    QVERIFY(store.verified().empty());
    QVERIFY(QFile::exists(saved.directory + "/screenplay.json"));
    // Later retention must preserve the changed package as evidence, even
    // though it is no longer offered as an intact recovery alternative.
    for (int i = 1; i < 5; ++i) store.preserve(opened.screenplay, "emergency", time.addSecs(i));
    QCOMPARE(open_project(saved.directory).screenplay.elements.front().text, std::string("Tampered."));
  }

  void failed_storage_keeps_the_existing_verified_copy() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("original"), {0, {{ElementType::action, "Before."}}});
    const auto screenplay = open_project(project.directory).screenplay;
    const auto root = parent.filePath("recovery");
    RecoveryStore store(project, {}, root);
    const auto previous = store.preserve(screenplay, "emergency");
    QVERIFY(QFile::setPermissions(root, QFile::ReadOwner | QFile::ExeOwner));
    auto restore = qScopeGuard([&] { QFile::setPermissions(root, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    QVERIFY_THROWS_EXCEPTION(SourceError, store.preserve(screenplay, "emergency"));
    QCOMPARE(store.verified().size(), 1u);
    QCOMPARE(store.verified().front().directory, previous.directory);
  }

  void retention_keeps_an_open_recovery_project() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("original"), {0, {{ElementType::action, "Before."}}});
    const auto screenplay = open_project(project.directory).screenplay;
    RecoveryStore store(project, {}, parent.filePath("recovery"));
    const auto time = QDateTime::fromString("2026-10-28T12:00:00.000Z", Qt::ISODateWithMs);
    const auto first = store.preserve(screenplay, "emergency", time);
    ProjectSession active(first.directory);
    for (int i = 1; i < 5; ++i) store.preserve(screenplay, "emergency", time.addSecs(i));
    QCOMPARE(store.verified().size(), 3u); // Two latest, plus the open package.
    active.check_destination();
  }

  void clock_rollback_never_prunes_the_copy_being_acknowledged() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("original"), {0, {{ElementType::action, "Before."}}});
    const auto screenplay = open_project(project.directory).screenplay;
    RecoveryStore store(project, {}, parent.filePath("recovery"));
    const auto time = QDateTime::fromString("2026-10-28T12:00:00.000Z", Qt::ISODateWithMs);
    for (int i = 0; i < 3; ++i) store.preserve(screenplay, "emergency", time.addSecs(i));
    const auto latest = store.preserve(screenplay, "emergency", time.addSecs(-10));
    QVERIFY(QFile::exists(latest.directory + "/screenplay.json"));
    const auto copies = store.verified();
    QCOMPARE(copies.size(), 2u);
    QVERIFY(std::any_of(copies.begin(), copies.end(), [&](const auto& copy) { return copy.directory == latest.directory; }));
  }

  void retention_keeps_two_latest_and_ten_older_minutes() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("original"), {0, {{ElementType::action, "Before."}}});
    RecoveryStore store(project, {}, parent.filePath("recovery"));
    auto time = QDateTime::fromString("2026-10-28T12:00:00.000Z", Qt::ISODateWithMs);
    for (int i = 0; i < 15; ++i)
      store.preserve({static_cast<std::uint64_t>(i), {{ElementType::action, "Writing.", false, {}, "0123456789abcdef"}}}, "autosave", time.addSecs(60 * i));
    store.preserve({15, {{ElementType::action, "Last.", false, {}, "0123456789abcdef"}}}, "autosave", time.addSecs(60 * 14 + 1));
    const auto copies = store.verified();
    QCOMPARE(copies.size(), 12u);
    QCOMPARE(copies.at(0).revision, 15u);
    QCOMPARE(copies.at(1).revision, 14u);
    QCOMPARE(copies.at(2).revision, 13u);
    QCOMPARE(copies.back().revision, 4u);
  }
};
QTEST_GUILESS_MAIN(RecoveryTests)
#include "recovery_tests.moc"
