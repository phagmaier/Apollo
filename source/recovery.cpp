#include "source/recovery.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <unistd.h>

namespace apollo::source {
namespace {
QString digest(const QByteArray& bytes) {
  return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
[[noreturn]] void fail(const QString& message) { throw SourceError(SourceError::Kind::io, message); }
QByteArray read(const QString& path) {
  if (!QFileInfo(path).isFile() || QFileInfo(path).isSymLink()) fail(QStringLiteral("invalid recovery file %1").arg(path));
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) fail(QStringLiteral("cannot read recovery file %1").arg(path));
  const auto bytes = file.readAll();
  if (file.error() != QFileDevice::NoError) fail(QStringLiteral("cannot read recovery file %1").arg(path));
  return bytes;
}
QJsonObject hashes(const QString& directory) {
  QJsonObject result;
  for (const auto* name : {"project.json", "screenplay.json", "imported-material.json", "imports/original.fountain"}) {
    const auto path = directory + '/' + name;
    if (QFileInfo::exists(path) || QFileInfo(path).isSymLink()) result.insert(name, digest(read(path)));
  }
  return result;
}
}
RecoveryStore::RecoveryStore(Project origin, ProjectAssets assets, QString directory)
  : origin_(std::move(origin)), assets_(std::move(assets)), directory_(std::move(directory)) {}

QString RecoveryStore::emergency_directory(const Project& origin) {
  const auto key = digest(origin.id.toUtf8() + '\n' + origin.directory.toUtf8());
  const auto data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (data.isEmpty()) fail(QStringLiteral("no application-data location is available for emergency copies"));
  return data + "/Apollo/recovery/" + key;
}

std::optional<RecoveryPackage> RecoveryStore::inspect_copy(const QString& path) {
  try {
    const QFileInfo folder(path);
    if (!folder.isDir() || folder.isSymLink()) return std::nullopt;
    const auto metadata_bytes = read(path + "/recovery.json");
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(metadata_bytes, &error);
    const auto metadata = doc.object();
    if (error.error != QJsonParseError::NoError || !doc.isObject() || metadata.size() != 7 ||
        metadata.value("format") != "apollo-recovery" || metadata.value("version") != 1 ||
        !metadata.value("hashes").isObject()) return std::nullopt;
    const auto origin_id = metadata.value("originId").toString();
    const auto origin_directory = metadata.value("originDirectory").toString();
    if (QUuid::fromString(origin_id).isNull() || QUuid::fromString(origin_id).toString(QUuid::WithoutBraces) != origin_id ||
        !QDir::isAbsolutePath(origin_directory) || QDir::cleanPath(origin_directory) != origin_directory) return std::nullopt;
    const auto captured = QDateTime::fromString(metadata.value("capturedAt").toString(), Qt::ISODateWithMs);
    const auto reason = metadata.value("reason").toString();
    if (!captured.isValid() || (reason != "autosave" && reason != "emergency" && reason != "reload")) return std::nullopt;
    const auto expected_hashes = metadata.value("hashes").toObject();
    if (!expected_hashes.contains("project.json") || !expected_hashes.contains("screenplay.json") ||
        expected_hashes != hashes(path)) return std::nullopt;
    auto project = open_project(path);
    if (project.project.screenplay_file != QLatin1String("screenplay.json") ||
        metadata_bytes != read(path + "/recovery.json") || expected_hashes != hashes(path)) return std::nullopt;
    const auto revision = project.screenplay.revision;
    return RecoveryPackage{{path, reason, captured.toUTC(), revision}, origin_id, origin_directory, std::move(project)};
  } catch (const SourceError&) { return std::nullopt; /* Preserve unreadable/corrupt evidence. */ }
}

std::optional<RecoveryCopy> RecoveryStore::verified_copy(const QString& path) const {
  const auto package = inspect_copy(path);
  if (!package || package->origin_id != origin_.id || package->origin_directory != origin_.directory) return std::nullopt;
  return package->copy;
}

std::vector<RecoveryCopy> RecoveryStore::verified() const {
  std::vector<RecoveryCopy> result;
  for (const auto& name : QDir(directory_).entryList({"copy-*"}, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
    if (auto copy = verified_copy(directory_ + '/' + name)) result.push_back(*copy);
  }
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    return a.captured_at != b.captured_at ? a.captured_at > b.captured_at : a.directory > b.directory;
  });
  return result;
}

RecoveryCopy RecoveryStore::preserve(const Screenplay& screenplay, const QString& reason, const QDateTime& captured_at) {
  if (!captured_at.isValid() || (reason != "autosave" && reason != "emergency" && reason != "reload"))
    fail(QStringLiteral("invalid recovery request"));
  if (directory_.isEmpty() || QFileInfo(directory_).isSymLink() || !QDir().mkpath(directory_))
    fail(QStringLiteral("cannot create recovery folder %1").arg(directory_));
  QLockFile lock(directory_ + "/.recovery.lock");
  lock.setStaleLockTime(0);
  if (!lock.tryLock(0)) fail(QStringLiteral("cannot lock recovery folder %1").arg(directory_));
  QTemporaryDir staging(directory_ + "/.apollo-recovery-staging-XXXXXX");
  if (!staging.isValid()) fail(QStringLiteral("cannot stage recovery in %1").arg(directory_));
  const auto package = staging.filePath("project");
  create_project(package, screenplay, assets_);
  QJsonObject metadata{{"format", "apollo-recovery"}, {"version", 1}, {"originId", origin_.id},
                       {"originDirectory", origin_.directory}, {"capturedAt", captured_at.toUTC().toString(Qt::ISODateWithMs)},
                       {"reason", reason}, {"hashes", hashes(package)}};
  replace_file(package + "/recovery.json", QJsonDocument(metadata).toJson());
  const auto reopened = open_project(package);
  if (reopened.screenplay != canonical(screenplay) || reopened.assets != assets_)
    fail(QStringLiteral("recovery did not read back in %1").arg(package));
  const auto destination = directory_ + "/copy-" + QString::number(captured_at.toMSecsSinceEpoch()) + '-' +
                           QUuid::createUuid().toString(QUuid::WithoutBraces);
  if (::renameat2(AT_FDCWD, QFile::encodeName(package).constData(), AT_FDCWD, QFile::encodeName(destination).constData(), RENAME_NOREPLACE) != 0)
    fail(QStringLiteral("cannot publish recovery %1: %2").arg(destination, QString::fromLocal8Bit(std::strerror(errno))));
  const RecoveryCopy copy{destination, reason, captured_at.toUTC(), screenplay.revision};
  // Prune only after a complete new copy exists. Retain two newest copies,
  // plus ten older minute buckets; never delete invalid/unrecognized evidence.
  const auto copies = verified();
  if (std::none_of(copies.begin(), copies.end(), [&](const auto& item) { return item.directory == destination; }))
    fail(QStringLiteral("published recovery could not be verified at %1").arg(destination));
  std::set<qint64> minutes;
  minutes.insert(captured_at.toMSecsSinceEpoch() / 60000);
  unsigned recent = 1;
  unsigned older = 0;
  for (std::size_t i = 0; i < copies.size(); ++i) {
    // Clock changes or an old queued capture must never prune the package
    // this call is about to acknowledge.
    if (copies[i].directory == destination) continue;
    const auto minute = copies[i].captured_at.toMSecsSinceEpoch() / 60000;
    if (recent < 2) { ++recent; minutes.insert(minute); continue; }
    if (older < 10 && minutes.insert(minute).second) { ++older; continue; }
    // A recovery package can be opened as an ordinary editable project.
    // Never prune it while another session may be using it; stale locks are
    // conservatively kept until the project-open flow recovers them.
    if (QFileInfo::exists(copies[i].directory + "/.apollo.lock")) continue;
    QLockFile project_lock(copies[i].directory + "/.apollo.lock");
    project_lock.setStaleLockTime(0);
    // Coordinate with an opener that arrives after the existence check.
    // Hold its edit lock until the package is no longer published.
    if (!project_lock.tryLock(0)) continue;
    // A session may have edited and closed this package since the scan.
    if (!verified_copy(copies[i].directory)) continue;
    const auto retired = directory_ + "/.apollo-recovery-pruning-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (::renameat2(AT_FDCWD, QFile::encodeName(copies[i].directory).constData(), AT_FDCWD,
                    QFile::encodeName(retired).constData(), RENAME_NOREPLACE) != 0) continue;
    // Recursive removal also removes the lock file. Unpublish first so a
    // new editor cannot acquire that name midway through recursive deletion.
    // Cleanup is best effort; failure does not invalidate the new package.
    QDir(retired).removeRecursively();
  }
  return copy;
}
}
