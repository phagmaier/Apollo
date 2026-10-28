#include "source/project_access.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <map>

using namespace apollo;
using namespace apollo::source;

namespace {
Screenplay writing(std::uint64_t revision, const char* text = "Current writing.") {
  return {revision, {{ElementType::action, text, false, {}, "e1"}}};
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QDateTime time_at(int minute = 0) {
  return QDateTime::fromString("2026-10-28T12:00:00.000Z", Qt::ISODateWithMs).addSecs(60 * minute);
}
std::map<QString, QByteArray> evidence(const QString& root) {
  std::map<QString, QByteArray> result;
  QDirIterator files(root, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
  while (files.hasNext()) {
    const auto path = files.next();
    if (!path.endsWith("/.apollo.lock")) result.emplace(QDir(root).relativeFilePath(path), bytes(path));
  }
  return result;
}
void update_hash(const QString& package, const QString& name) {
  auto metadata = QJsonDocument::fromJson(bytes(package + "/recovery.json")).object();
  auto hashes = metadata.value("hashes").toObject();
  hashes.insert(name, QString::fromLatin1(QCryptographicHash::hash(bytes(package + '/' + name), QCryptographicHash::Sha256).toHex()));
  metadata.insert("hashes", hashes);
  replace_file(package + "/recovery.json", QJsonDocument(metadata).toJson());
}
}

class ProjectAccessTests : public QObject {
  Q_OBJECT
  QTemporaryDir data_;
  QByteArray previous_data_;
  bool had_data_ = false;
private slots:
  void initTestCase() {
    QVERIFY(data_.isValid());
    had_data_ = qEnvironmentVariableIsSet("XDG_DATA_HOME");
    previous_data_ = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", data_.path().toUtf8());
  }
  void cleanupTestCase() {
    if (had_data_) qputenv("XDG_DATA_HOME", previous_data_); else qunsetenv("XDG_DATA_HOME");
  }

  void healthy_current_ignores_routine_autosaves() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore local(project, {}, project.directory + "/.recovery");
    local.preserve(writing(7, "Previous writing."), "autosave", time_at(1));
    local.preserve(writing(8), "autosave", time_at());
    const auto original = evidence(project.directory);
    ProjectAccess access(project.directory);
    QVERIFY2(access.current_available(), qPrintable(access.error()));
    QCOMPARE(access.project_id(), project.id);
    QCOMPARE(access.current_revision(), 8u);
    QVERIFY(access.error().isEmpty());
    QVERIFY(access.candidates().empty());
    auto session = access.open_current();
    QCOMPARE(session->opened().screenplay.revision, 8u);
    QCOMPARE(session->opened().screenplay.elements.front().text, std::string("Current writing."));
    session->check_destination();
    QVERIFY(evidence(project.directory) == original);
    QVERIFY_THROWS_EXCEPTION(SourceError, access.open_current());
  }

  void emergency_reconciliation_uses_revision_not_clock() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(7, "Older writing."), "emergency", time_at(30));
    const auto newer = emergency.preserve(writing(9, "Newer writing."), "emergency", time_at(-30));
    const auto conflict = emergency.preserve(writing(8, "Conflicting writing."), "emergency", time_at(40));
    ProjectAccess access(project.directory);
    QVERIFY(access.current_available());
    QCOMPARE(access.candidates().size(), 2u);
    QCOMPARE(access.candidates().at(0).copy.directory, newer.directory);
    QVERIFY(access.candidates().at(0).relation.contains("newer", Qt::CaseInsensitive));
    QCOMPARE(access.candidates().at(1).copy.directory, conflict.directory);
    QVERIFY(access.candidates().at(1).relation.contains("conflict", Qt::CaseInsensitive));
    QCOMPARE(access.open_current()->opened().screenplay.revision, 8u);
  }

  void equal_revision_asset_changes_are_conflicts() {
    QTemporaryDir parent;
    const ProjectAssets assets{QByteArray("notes"), QByteArray("original")};
    const auto project = create_project(parent.filePath("project"), writing(8), assets);
    RecoveryStore emergency(project, {QByteArray("different notes"), QByteArray("original")}, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(8), "emergency", time_at());
    ProjectAccess access(project.directory);
    QCOMPARE(access.candidates().size(), 1u);
    QVERIFY(access.candidates().front().relation.contains("conflict", Qt::CaseInsensitive));
    QCOMPARE(*access.candidates().front().assets.imported_material, QByteArray("different notes"));
  }

  void damaged_current_offers_complete_alternatives_data() {
    QTest::addColumn<QString>("damage");
    for (const auto* value : {"source-corrupt", "source-missing", "manifest-corrupt", "manifest-missing"})
      QTest::newRow(value) << QString::fromLatin1(value);
  }
  void damaged_current_offers_complete_alternatives() {
    QFETCH(QString, damage);
    QTemporaryDir parent;
    const ProjectAssets assets{QByteArray("notes\r\n"), QByteArray("Title: Original\r\n")};
    const auto project = create_project(parent.filePath("project"), writing(10), assets);
    RecoveryStore local(project, assets, project.directory + "/.recovery");
    local.preserve(writing(6, "Older alternative."), "autosave", time_at(5));
    RecoveryStore emergency(project, assets, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(9, "Latest alternative."), "emergency", time_at(-5));
    const auto path = damage.startsWith("source") ? project.screenplay_path() : project.directory + "/project.json";
    if (damage.endsWith("missing")) QVERIFY(QFile::remove(path)); else replace_file(path, "{damaged");
    const auto original = evidence(project.directory);
    ProjectAccess access(project.directory);
    QVERIFY(!access.current_available());
    QVERIFY(!access.error().isEmpty());
    QCOMPARE(access.project_id(), project.id);
    QCOMPARE(access.candidates().size(), 2u);
    QCOMPARE(access.candidates().front().copy.revision, 9u);
    QVERIFY_THROWS_EXCEPTION(SourceError, access.open_current());
    const auto destination = parent.filePath("recovered");
    const auto package_before = evidence(access.candidates().front().copy.directory);
    auto recovered = access.recover(0, destination);
    QCOMPARE(recovered->opened().project.directory, destination);
    QVERIFY(recovered->opened().project.id != project.id);
    QCOMPARE(recovered->opened().screenplay.elements.front().text, std::string("Latest alternative."));
    QVERIFY(recovered->opened().assets == assets);
    recovered->check_destination();
    QVERIFY(evidence(project.directory) == original);
    QVERIFY(evidence(access.candidates().front().copy.directory) == package_before);
  }

  void no_valid_alternative_preserves_the_failure() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(1));
    replace_file(project.screenplay_path(), "not JSON");
    ProjectAccess access(project.directory);
    QVERIFY(!access.current_available());
    QVERIFY(!access.error().isEmpty());
    QVERIFY(access.candidates().empty());
    QVERIFY_THROWS_EXCEPTION(SourceError, access.open_current());
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, parent.filePath("never-created")));
    QVERIFY(!QFile::exists(parent.filePath("never-created")));
    QCOMPARE(bytes(project.screenplay_path()), QByteArray("not JSON"));
  }

  void invalid_packages_and_dependencies_are_not_offered_data() {
    QTest::addColumn<QString>("damage");
    for (const auto* value : {"hash", "dependency-missing", "dependency-symlink", "new-project-version", "new-source-version", "new-recovery-version"})
      QTest::newRow(value) << QString::fromLatin1(value);
  }
  void invalid_packages_and_dependencies_are_not_offered() {
    QFETCH(QString, damage);
    QTemporaryDir parent;
    const ProjectAssets assets{QByteArray("notes"), QByteArray("original")};
    const auto project = create_project(parent.filePath("project"), writing(1), assets);
    RecoveryStore store(project, assets, project.directory + "/.recovery");
    const auto copy = store.preserve(writing(2), "autosave", time_at());
    if (damage == "hash") replace_file(copy.directory + "/screenplay.json", serialize_screenplay(writing(3)));
    else if (damage == "dependency-missing") QVERIFY(QFile::remove(copy.directory + "/imports/original.fountain"));
    else if (damage == "dependency-symlink") {
      QVERIFY(QFile::remove(copy.directory + "/imports/original.fountain"));
      QVERIFY(QFile::link(project.directory + "/imports/original.fountain", copy.directory + "/imports/original.fountain"));
    } else {
      const auto name = damage == "new-project-version" ? QStringLiteral("project.json") :
                        damage == "new-source-version" ? QStringLiteral("screenplay.json") : QStringLiteral("recovery.json");
      auto json = QJsonDocument::fromJson(bytes(copy.directory + '/' + name)).object();
      json.insert("version", 99);
      replace_file(copy.directory + '/' + name, QJsonDocument(json).toJson());
      if (name != "recovery.json") update_hash(copy.directory, name);
    }
    replace_file(project.screenplay_path(), "damaged");
    const auto before = evidence(project.directory);
    ProjectAccess access(project.directory);
    QVERIFY(!access.current_available());
    QVERIFY(access.candidates().empty());
    QVERIFY(evidence(project.directory) == before);
  }

  void moved_project_discovers_local_and_emergency_by_identity() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore local(project, {}, project.directory + "/.recovery");
    local.preserve(writing(10), "autosave", time_at());
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(9), "emergency", time_at(1));
    const auto moved = parent.filePath("moved");
    QVERIFY(QDir().rename(project.directory, moved));
    ProjectAccess access(moved);
    QVERIFY(access.current_available());
    QCOMPARE(access.candidates().size(), 2u);
    QCOMPARE(access.candidates().front().origin_directory, project.directory);
    QCOMPARE(access.open_current()->opened().project.directory, moved);
  }

  void missing_folder_uses_saved_identity_or_matching_verified_origin() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(9), "emergency", time_at());
    QVERIFY(QDir().rename(project.directory, parent.filePath("moved")));
    for (const auto& id : {QString(), project.id}) {
      ProjectAccess access(project.directory, id);
      QVERIFY(!access.current_available());
      QVERIFY(access.error().contains("folder", Qt::CaseInsensitive));
      QCOMPARE(access.project_id(), project.id);
      QCOMPARE(access.candidates().size(), 1u);
    }
    ProjectAccess unknown(parent.filePath("missing.fountain"));
    QVERIFY(!unknown.current_available());
    QVERIFY(!unknown.error().isEmpty());
    QVERIFY(unknown.candidates().empty());
  }

  void competing_original_lock_blocks_recovery_even_when_damaged() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(1));
    RecoveryStore store(project, {}, project.directory + "/.recovery");
    store.preserve(writing(2), "autosave", time_at());
    replace_file(project.screenplay_path(), "damaged");
    ProjectAccess first(project.directory);
    QCOMPARE(first.candidates().size(), 1u);
    ProjectAccess second(project.directory);
    QVERIFY(!second.current_available());
    QVERIFY(second.error().contains("locked", Qt::CaseInsensitive));
    QVERIFY(second.candidates().empty());
    QVERIFY_THROWS_EXCEPTION(SourceError, second.recover(0, parent.filePath("blocked")));
    QVERIFY_THROWS_EXCEPTION(SourceError, ProjectSession{project.directory});
    auto recovered = first.recover(0, parent.filePath("recovered"));
    QVERIFY(recovered != nullptr);
  }

  void stale_current_and_selected_packages_are_refused_data() {
    QTest::addColumn<QString>("change");
    for (const auto* value : {"current", "package", "metadata", "pruned", "locked-package", "lost-original-lock"})
      QTest::newRow(value) << QString::fromLatin1(value);
  }
  void stale_current_and_selected_packages_are_refused() {
    QFETCH(QString, change);
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(1));
    RecoveryStore store(project, {}, project.directory + "/.recovery");
    const auto copy = store.preserve(writing(2), "autosave", time_at());
    ProjectAccess access(project.directory);
    QCOMPARE(access.candidates().size(), 1u);
    std::unique_ptr<ProjectSession> package_lock;
    if (change == "current") replace_file(project.screenplay_path(), serialize_screenplay(writing(3)));
    else if (change == "package") {
      replace_file(copy.directory + "/screenplay.json", serialize_screenplay(writing(4)));
      update_hash(copy.directory, "screenplay.json");
    } else if (change == "metadata") {
      auto json = QJsonDocument::fromJson(bytes(copy.directory + "/recovery.json")).object();
      json.insert("capturedAt", time_at(50).toString(Qt::ISODateWithMs));
      replace_file(copy.directory + "/recovery.json", QJsonDocument(json).toJson());
    } else if (change == "pruned") QVERIFY(QDir(copy.directory).removeRecursively());
    else if (change == "locked-package") package_lock = std::make_unique<ProjectSession>(copy.directory);
    else QVERIFY(QFile::remove(project.directory + "/.apollo.lock"));
    if (change == "current") QVERIFY_THROWS_EXCEPTION(SourceError, access.open_current());
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, parent.filePath("refused")));
    QVERIFY(!QFile::exists(parent.filePath("refused")));
  }

  void recovery_destination_must_be_absolute_and_new() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(1));
    RecoveryStore store(project, {}, project.directory + "/.recovery");
    store.preserve(writing(2), "autosave", time_at());
    ProjectAccess access(project.directory);
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, "relative-recovery"));
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, project.directory));
    const auto existing = parent.filePath("empty");
    QVERIFY(QDir().mkdir(existing));
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, existing));
    QCOMPARE(open_project(project.directory).screenplay.revision, 1u);
  }

  void newer_current_versions_are_refused_without_rewriting() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(1));
    auto manifest = QJsonDocument::fromJson(bytes(project.directory + "/project.json")).object();
    manifest.insert("version", 99);
    replace_file(project.directory + "/project.json", QJsonDocument(manifest).toJson());
    const auto before = evidence(project.directory);
    ProjectAccess access(project.directory);
    QVERIFY(!access.current_available());
    QVERIFY(access.error().contains("newer Apollo"));
    QVERIFY_THROWS_EXCEPTION(SourceError, access.open_current());
    QVERIFY(evidence(project.directory) == before);
  }

  void saved_identity_never_adopts_a_replacement_project() {
    QTemporaryDir parent;
    const auto original = create_project(parent.filePath("original"), writing(8));
    RecoveryStore emergency(original, {}, RecoveryStore::emergency_directory(original));
    emergency.preserve(writing(9, "Saved project's writing."), "emergency", time_at());
    const auto replacement = create_project(parent.filePath("replacement"), writing(2, "Different project."));
    const auto before = evidence(replacement.directory);
    ProjectAccess access(replacement.directory, original.id);
    QVERIFY(!access.current_available());
    QVERIFY(access.error().contains("identity", Qt::CaseInsensitive));
    QCOMPARE(access.project_id(), original.id);
    QCOMPARE(access.candidates().size(), 1u);
    QVERIFY_THROWS_EXCEPTION(SourceError, access.open_current());
    auto recovered = access.recover(0, parent.filePath("recovered"));
    QCOMPARE(recovered->opened().screenplay.elements.front().text, std::string("Saved project's writing."));
    QVERIFY(evidence(replacement.directory) == before);
  }

  void saved_identity_finds_emergency_after_a_location_change() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("original"), writing(8));
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(9), "emergency", time_at());
    const auto missing = parent.filePath("new-missing-location");
    ProjectAccess unknown(missing);
    QVERIFY(unknown.candidates().empty());
    ProjectAccess saved(missing, project.id);
    QCOMPARE(saved.candidates().size(), 1u);
    QCOMPARE(saved.candidates().front().origin_directory, project.directory);
    auto recovered = saved.recover(0, parent.filePath("recovered"));
    QCOMPARE(recovered->opened().screenplay.revision, 9u);
  }

  void a_missing_original_reappearing_invalidates_the_decision() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("original"), writing(8));
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(9), "emergency", time_at());
    QVERIFY(QDir().rename(project.directory, parent.filePath("moved")));
    ProjectAccess access(project.directory, project.id);
    QCOMPARE(access.candidates().size(), 1u);
    const auto replacement = create_project(project.directory, writing(10, "Newly arrived project."));
    ProjectSession active(replacement.directory);
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, parent.filePath("refused")));
    QCOMPARE(active.opened().screenplay.elements.front().text, std::string("Newly arrived project."));
    QVERIFY(!QFile::exists(parent.filePath("refused")));
  }

  void changes_to_damaged_originals_invalidate_recovery_data() {
    QTest::addColumn<QString>("damage");
    for (const auto* value : {"source-corrupt", "source-missing", "manifest-corrupt", "manifest-missing",
                              "dependency-missing", "dependency-unreadable", "dependency-symlink"})
      QTest::newRow(value) << QString::fromLatin1(value);
  }
  void changes_to_damaged_originals_invalidate_recovery() {
    QFETCH(QString, damage);
    QTemporaryDir parent;
    const ProjectAssets assets{QByteArray("notes"), QByteArray("original")};
    const auto project = create_project(parent.filePath("project"), writing(10), assets);
    RecoveryStore store(project, assets, project.directory + "/.recovery");
    const auto copy = store.preserve(writing(9, "Older alternative."), "autosave", time_at());
    const auto manifest = bytes(project.directory + "/project.json");
    const auto dependency = project.directory + "/imports/original.fountain";
    auto permissions = qScopeGuard([&] {
      QFile::setPermissions(dependency, QFile::ReadOwner | QFile::WriteOwner);
    });
    if (damage == "source-corrupt" || damage == "dependency-missing") replace_file(project.screenplay_path(), "damaged");
    if (damage == "source-missing") QVERIFY(QFile::remove(project.screenplay_path()));
    if (damage == "manifest-corrupt") replace_file(project.directory + "/project.json", "damaged");
    if (damage == "manifest-missing") QVERIFY(QFile::remove(project.directory + "/project.json"));
    if (damage == "dependency-missing") QVERIFY(QFile::remove(dependency));
    if (damage == "dependency-unreadable") QVERIFY(QFile::setPermissions(dependency, {}));
    if (damage == "dependency-symlink") {
      QVERIFY(QFile::remove(dependency));
      const auto target = parent.filePath("external-original");
      replace_file(target, "original");
      QVERIFY(QFile::link(target, dependency));
    }
    ProjectAccess access(project.directory);
    QVERIFY(!access.current_available());
    QCOMPARE(access.candidates().size(), 1u);
    if (damage.startsWith("source")) replace_file(project.screenplay_path(), serialize_screenplay(writing(20, "Restored newer writing.")));
    else if (damage.startsWith("manifest")) replace_file(project.directory + "/project.json", manifest);
    else if (damage == "dependency-unreadable") QVERIFY(QFile::setPermissions(dependency, QFile::ReadOwner | QFile::WriteOwner));
    else {
      if (damage == "dependency-symlink") QVERIFY(QFile::remove(dependency));
      replace_file(dependency, "restored original");
    }
    const auto original_after_restore = evidence(project.directory);
    const auto package_before = evidence(copy.directory);
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, parent.filePath("refused")));
    QVERIFY(!QFile::exists(parent.filePath("refused")));
    QVERIFY(evidence(project.directory) == original_after_restore);
    QVERIFY(evidence(copy.directory) == package_before);
  }

  void file_replacing_the_original_still_allows_explicit_emergency_recovery() {
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    const auto copy = emergency.preserve(writing(9, "Emergency writing."), "emergency", time_at());
    QVERIFY(QDir().rename(project.directory, parent.filePath("moved")));
    replace_file(project.directory, "Retained non-project evidence.");
    const auto package_before = evidence(copy.directory);
    ProjectAccess access(project.directory, project.id);
    QVERIFY(!access.current_available());
    QVERIFY(!access.error().isEmpty());
    QCOMPARE(access.project_id(), project.id);
    QCOMPARE(access.candidates().size(), 1u);
    auto recovered = access.recover(0, parent.filePath("recovered"));
    QCOMPARE(recovered->opened().screenplay.elements.front().text, std::string("Emergency writing."));
    QVERIFY(recovered->opened().project.id != project.id);
    QCOMPARE(bytes(project.directory), QByteArray("Retained non-project evidence."));
    QVERIFY(evidence(copy.directory) == package_before);
  }

  void changed_non_directory_original_invalidates_recovery_data() {
    QTest::addColumn<QString>("change");
    for (const auto* value : {"replace", "rewrite", "remove", "directory"})
      QTest::newRow(value) << QString::fromLatin1(value);
  }
  void changed_non_directory_original_invalidates_recovery() {
    QFETCH(QString, change);
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    emergency.preserve(writing(9), "emergency", time_at());
    QVERIFY(QDir().rename(project.directory, parent.filePath("moved")));
    replace_file(project.directory, "Evidence at inspection.");
    ProjectAccess access(project.directory, project.id);
    QCOMPARE(access.candidates().size(), 1u);
    if (change == "replace") replace_file(project.directory, "Replacement evidence.");
    else if (change == "rewrite") {
      QFile file(project.directory);
      QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
      QCOMPARE(file.write("Changed in-place."), 17);
    } else {
      QVERIFY(QFile::remove(project.directory));
      if (change == "directory") create_project(project.directory, writing(20, "Replacement project."));
    }
    const auto before = bytes(project.directory);
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, parent.filePath("refused")));
    QVERIFY(!QFile::exists(parent.filePath("refused")));
    QCOMPARE(bytes(project.directory), before);
  }

  void post_publication_failure_reports_and_retains_the_new_project_data() {
    QTest::addColumn<bool>("restore_original");
    QTest::newRow("original-restored-during-staging") << true;
    QTest::newRow("published-copy-locked-before-activation") << false;
  }
  void post_publication_failure_reports_and_retains_the_new_project() {
    QFETCH(bool, restore_original);
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore store(project, {}, project.directory + "/.recovery");
    const auto copy = store.preserve(writing(9, "Recovered alternative."), "autosave", time_at());
    replace_file(project.screenplay_path(), "damaged");
    ProjectAccess access(project.directory);
    QCOMPARE(access.candidates().size(), 1u);
    const auto package_before = evidence(copy.directory);
    const auto destination = parent.filePath("retained");
    std::unique_ptr<ProjectSession> competing;
    QString failure;
    try {
      access.recover(0, destination, [&] {
        if (restore_original) replace_file(project.screenplay_path(), serialize_screenplay(writing(20, "Restored newest writing.")));
        else competing = std::make_unique<ProjectSession>(destination);
      });
    } catch (const SourceError& error) { failure = QString::fromUtf8(error.what()); }
    QVERIFY(!failure.isEmpty());
    QVERIFY(failure.contains(destination));
    QVERIFY(failure.contains("retained", Qt::CaseInsensitive));
    const auto retained = open_project(destination);
    QCOMPARE(retained.screenplay.elements.front().text, std::string("Recovered alternative."));
    QVERIFY(retained.project.id != project.id);
    QCOMPARE(bytes(project.screenplay_path()), restore_original ? serialize_screenplay(writing(20, "Restored newest writing.")) : QByteArray("damaged"));
    QVERIFY(evidence(copy.directory) == package_before);
  }

  void dangling_original_link_reappearing_invalidates_recovery_data() {
    QTest::addColumn<bool>("directory");
    QTest::newRow("locked-directory") << true;
    QTest::newRow("regular-file") << false;
  }
  void dangling_original_link_reappearing_invalidates_recovery() {
    QFETCH(bool, directory);
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("project"), writing(8));
    RecoveryStore emergency(project, {}, RecoveryStore::emergency_directory(project));
    const auto copy = emergency.preserve(writing(9, "Recovered alternative."), "emergency", time_at());
    const auto package_before = evidence(copy.directory);
    const auto alias = parent.filePath("alias");
    QVERIFY(QFile::link(project.directory, alias));
    const auto moved = parent.filePath("moved");
    QVERIFY(QDir().rename(project.directory, moved));
    ProjectAccess access(alias, project.id);
    QCOMPARE(access.candidates().size(), 1u);
    QVERIFY(!access.current_available());
    std::unique_ptr<ProjectSession> active;
    if (directory) {
      QVERIFY(QDir().rename(moved, project.directory));
      active = std::make_unique<ProjectSession>(project.directory);
    } else replace_file(project.directory, "Retained replacement file.");
    const auto original_before = evidence(directory ? project.directory : moved);
    const auto destination = parent.filePath("refused");
    QVERIFY_THROWS_EXCEPTION(SourceError, access.recover(0, destination));
    QVERIFY(!QFileInfo::exists(destination));
    if (active) active->check_destination();
    else QCOMPARE(bytes(project.directory), QByteArray("Retained replacement file."));
    QVERIFY(evidence(directory ? project.directory : moved) == original_before);
    QVERIFY(evidence(copy.directory) == package_before);
  }
};
QTEST_GUILESS_MAIN(ProjectAccessTests)
#include "project_access_tests.moc"
