#include "source/project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace apollo::source {
namespace {
const QString manifest_name = QStringLiteral("project.json"), default_screenplay = QStringLiteral("screenplay.json");

[[noreturn]] void io_error(const QString& message) { throw SourceError(SourceError::Kind::io, message); }
[[noreturn]] void malformed(const QString& message) { throw SourceError(SourceError::Kind::malformed, message); }

QByteArray read_all(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) io_error(QStringLiteral("cannot read %1: %2").arg(path, file.errorString()));
  auto bytes = file.readAll();
  if (file.error() != QFileDevice::NoError) io_error(QStringLiteral("cannot read %1: %2").arg(path, file.errorString()));
  return bytes;
}

QByteArray manifest(const QString& id, const QString& screenplay_file) {
  return "{\n  \"format\": \"apollo-project\",\n  \"version\": " + QByteArray::number(project_version) + ",\n  \"id\": \"" +
         id.toUtf8() + "\",\n  \"screenplay\": \"" + screenplay_file.toUtf8() + "\"\n}\n";
}

// project.json; its screenplay is read separately.
Project read_manifest(const QString& directory, const QByteArray& bytes) {
  const auto path = directory + QLatin1Char('/') + manifest_name;
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(bytes, &error);
  const auto root = json.object();
  if (error.error != QJsonParseError::NoError || !json.isObject() || root.value("format").toString() != QLatin1String("apollo-project"))
    malformed(QStringLiteral("%1 is not an Apollo project file").arg(path));
  const auto version = root.value("version");
  if (version.isDouble() && version.toDouble() > project_version)
    throw SourceError(SourceError::Kind::newer_version, QStringLiteral("project format version %1 needs a newer Apollo").arg(version.toDouble()));
  if (!version.isDouble() || version.toDouble() != project_version) malformed(QStringLiteral("unsupported project format version in %1").arg(path));
  if (root.size() != 4 || !root.contains("id") || !root.contains("screenplay")) malformed(QStringLiteral("unexpected fields in %1").arg(path));
  const auto id = root.value("id").toString(), screenplay = root.value("screenplay").toString();
  // The canonical lowercase form, so one project has one spelling.
  if (QUuid::fromString(id).isNull() || QUuid::fromString(id).toString(QUuid::WithoutBraces) != id)
    malformed(QStringLiteral("invalid project id in %1").arg(path));
  if (!screenplay.endsWith(QLatin1String(".json")) || screenplay.startsWith(QLatin1Char('.')) || screenplay.contains(QLatin1Char('/')) ||
      screenplay.contains(QLatin1Char('\\')))
    malformed(QStringLiteral("invalid screenplay file name in %1").arg(path));
  return {directory, id, screenplay};
}

ProjectAssets read_assets(const Project& project) {
  auto dependency = [&](const QString& name) -> std::optional<QByteArray> {
    const auto path = project.directory + '/' + name;
    const QFileInfo info(path);
    if (info.isSymLink() || (info.exists() && !info.isFile())) io_error(QStringLiteral("import dependency %1 must be a regular file").arg(path));
    return info.exists() ? std::optional(read_all(path)) : std::nullopt;
  };
  ProjectAssets assets;
  if (project.screenplay_file != QLatin1String("imported-material.json")) assets.imported_material = dependency("imported-material.json");
  const auto imports = QFileInfo(project.directory + "/imports");
  if (imports.isSymLink() || (imports.exists() && !imports.isDir())) io_error(QStringLiteral("import folder %1 must be a directory").arg(imports.filePath()));
  assets.original_fountain = dependency("imports/original.fountain");
  return assets;
}
}

void replace_file(const QString& path, const QByteArray& bytes, const std::function<void(WritePhase)>& hook) {
  QSaveFile file(path);
  // Writing in place would lose the complete old file if this write stopped.
  file.setDirectWriteFallback(false);
  auto fail = [&](const QString& reason) { io_error(QStringLiteral("cannot save %1: %2").arg(path, reason)); };
  if (!file.open(QIODevice::WriteOnly)) fail(file.errorString());
  // Leaving without commit() discards the temporary file.
  try {
    if (hook) hook(WritePhase::opened);
    if (file.write(bytes) != bytes.size()) fail(file.errorString());
    if (hook) hook(WritePhase::written);
  } catch (const SourceError&) {
    throw;
  } catch (const std::exception& error) {
    fail(QString::fromUtf8(error.what()));
  }
  if (!file.commit()) fail(file.errorString());
  if (hook) hook(WritePhase::committed);
}

OpenedProject open_project(const QString& directory) {
  const auto absolute = QFileInfo(directory).absoluteFilePath();
  if (!QFileInfo(absolute).isDir()) io_error(QStringLiteral("project folder %1 does not exist").arg(absolute));
  auto project = read_manifest(absolute, read_all(absolute + '/' + manifest_name));
  auto screenplay = parse_screenplay(read_all(project.screenplay_path()));
  auto assets = read_assets(project);
  return {std::move(project), std::move(screenplay), std::move(assets)};
}

Project create_project(const QString& destination, Screenplay screenplay, const ProjectAssets& assets) {
  const QFileInfo target(destination);
  const auto parent = target.absolutePath(), path = target.absoluteFilePath();
  if (target.exists() || target.isSymLink()) io_error(QStringLiteral("%1 already exists").arg(path));
  if (!QFileInfo(parent).isDir()) io_error(QStringLiteral("folder %1 does not exist").arg(parent));
  for (auto& element : screenplay.elements) if (element.id.empty()) element.id = new_element_id();
  QByteArray source;
  try {
    source = serialize_screenplay(screenplay);
  } catch (const std::invalid_argument& error) {
    malformed(QString::fromUtf8(error.what()));
  }
  QTemporaryDir staging(parent + QStringLiteral("/.apollo-staging-XXXXXX"));
  if (!staging.isValid()) io_error(QStringLiteral("cannot create a staging folder in %1: %2").arg(parent, staging.errorString()));
  const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  replace_file(staging.filePath(default_screenplay), source);
  replace_file(staging.filePath(manifest_name), manifest(id, default_screenplay));
  if (assets.imported_material) replace_file(staging.filePath("imported-material.json"), *assets.imported_material);
  if (assets.original_fountain) {
    if (!QDir(staging.path()).mkdir("imports")) io_error(QStringLiteral("cannot create staged import folder"));
    replace_file(staging.filePath("imports/original.fountain"), *assets.original_fountain);
  }
  // Publish only what reads back as a complete project.
  const auto verified = open_project(staging.path());
  if (!(verified.screenplay == canonical(screenplay)) || verified.assets != assets) malformed(QStringLiteral("the staged project did not read back"));
  // QTemporaryDir is private to its owner; give the project ordinary folder access.
  QFile::setPermissions(staging.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup |
                                        QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
  if (::renameat2(AT_FDCWD, QFile::encodeName(staging.path()).constData(), AT_FDCWD, QFile::encodeName(path).constData(),
                  RENAME_NOREPLACE) != 0)
    io_error(QStringLiteral("cannot create %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno))));
  staging.setAutoRemove(false);
  return {path, id, default_screenplay};
}

namespace {
[[noreturn]] void destination_changed(const QString& path) {
  throw SourceError(SourceError::Kind::external_change,
                    QStringLiteral("Saving paused: %1 changed, is missing, or cannot be verified. "
                                   "Restore the original destination before Retry save.").arg(path));
}
}

ProjectSession::Identity ProjectSession::identity(const QString& path, bool directory) {
  struct stat info{};
  // Reject file symlinks and hard links: two separate project locks cannot
  // coordinate writes to shared authoritative files. Folder aliases are
  // resolved once, before locking.
  if (::lstat(QFile::encodeName(path).constData(), &info) != 0 ||
      (directory ? !S_ISDIR(info.st_mode) : (!S_ISREG(info.st_mode) || info.st_nlink != 1)))
    destination_changed(path);
  return {static_cast<quint64>(info.st_dev), static_cast<quint64>(info.st_ino)};
}

ProjectSession::FileState ProjectSession::read_file(const QString& path) {
  const auto before = identity(path);
  QByteArray bytes;
  try { bytes = read_all(path); }
  catch (const SourceError&) { destination_changed(path); }
  if (identity(path) != before) destination_changed(path);
  return {before, bytes};
}

ProjectSession::ProjectSession(const QString& directory) : ProjectSession(directory, InspectionOnly{}) {
  load_current();
}

ProjectSession::ProjectSession(const QString& directory, InspectionOnly) {
  const auto canonical = QFileInfo(directory).canonicalFilePath();
  if (canonical.isEmpty() || !QFileInfo(canonical).isDir()) io_error(QStringLiteral("project folder %1 does not exist").arg(directory));
  directory_identity_ = identity(canonical, true);
  opened_.project.directory = canonical;
  // Anchor lock cleanup to the opened directory, even if it is renamed and
  // another project appears at the old path. Never unlink that project's lock.
  const auto fd = ::open(QFile::encodeName(canonical).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) io_error(QStringLiteral("cannot open project folder %1").arg(canonical));
  directory_handle_ = std::make_unique<QFile>();
  if (!directory_handle_->open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(fd);
    io_error(QStringLiteral("cannot hold project folder %1").arg(canonical));
  }
  lock_ = std::make_unique<QLockFile>(QStringLiteral("/proc/self/fd/%1/.apollo.lock").arg(fd));
  lock_->setStaleLockTime(0);
  if (!lock_->tryLock(0)) {
    if (lock_->error() != QLockFile::LockFailedError) io_error(QStringLiteral("cannot lock project %1").arg(canonical));
    qint64 pid = 0;
    QString host, app;
    lock_->getLockInfo(&pid, &host, &app);
    throw SourceError(SourceError::Kind::locked,
                      QStringLiteral("project %1 is already open or locked (%2, process %3 on %4)").arg(canonical, app).arg(pid).arg(host));
  }
  lock_file_ = read_file(lock_->fileName());
  check_lock();
}

void ProjectSession::load_current() {
  check_lock();
  const auto& canonical = opened_.project.directory;
  manifest_ = read_file(canonical + '/' + manifest_name);
  opened_.project = read_manifest(canonical, manifest_.bytes);
  screenplay_ = read_file(opened_.project.screenplay_path());
  opened_.screenplay = parse_screenplay(screenplay_.bytes);
  // Read dependencies while the locked original is available. Later copy
  // requests use this snapshot, never a changed or missing original folder.
  opened_.assets = read_assets(opened_.project);
  check_destination();
}

ProjectSession::~ProjectSession() = default;

void ProjectSession::check_lock() const {
  const auto& project = opened_.project;
  if (identity(project.directory, true) != directory_identity_) destination_changed(project.directory);
  try {
    if (read_file(lock_->fileName()) != lock_file_) destination_changed(lock_->fileName());
  } catch (const SourceError&) { destination_changed(project.directory + "/.apollo.lock"); }
}

void ProjectSession::check_destination() const {
  check_lock();
  const auto& project = opened_.project;
  if (read_file(project.directory + '/' + manifest_name) != manifest_) destination_changed(project.directory + '/' + manifest_name);
  if (read_file(project.screenplay_path()) != screenplay_) destination_changed(project.screenplay_path());
}

void ProjectSession::save(const QByteArray& bytes, const std::function<void(WritePhase)>& hook) {
  check_destination();
  replace_file(opened_.project.screenplay_path(), bytes, [&](WritePhase phase) {
    if (phase == WritePhase::committed) {
      // Record only the file containing our bytes, never silently adopt a
      // noncooperating writer's later replacement as the expected destination.
      auto current = read_file(opened_.project.screenplay_path());
      if (current.bytes != bytes) destination_changed(opened_.project.screenplay_path());
      screenplay_ = std::move(current);
    }
    if (hook) hook(phase);
    if (phase == WritePhase::written || phase == WritePhase::committed) check_destination();
  });
}

OpenedProject ProjectSession::read_reload() const {
  const auto& project = opened_.project;
  if (identity(project.directory, true) != directory_identity_ || read_file(lock_->fileName()) != lock_file_)
    destination_changed(project.directory);
  const auto manifest = read_file(project.directory + '/' + manifest_name);
  const auto current = read_manifest(project.directory, manifest.bytes);
  // A replaced project identity requires the explicit open/copy workflow.
  if (current.id != project.id) destination_changed(project.directory + '/' + manifest_name);
  const auto screenplay = read_file(current.screenplay_path());
  OpenedProject candidate{current, parse_screenplay(screenplay.bytes), read_assets(current)};
  if (read_file(project.directory + '/' + manifest_name) != manifest || read_file(current.screenplay_path()) != screenplay)
    destination_changed(project.directory);
  return candidate;
}

void ProjectSession::accept_reload(const OpenedProject& candidate) {
  const auto current = read_reload();
  if (current.project.id != candidate.project.id || current.project.screenplay_file != candidate.project.screenplay_file ||
      current.screenplay != candidate.screenplay || current.assets != candidate.assets)
    destination_changed(candidate.project.directory);
  const auto manifest = read_file(current.project.directory + '/' + manifest_name);
  const auto screenplay = read_file(current.project.screenplay_path());
  const auto manifest_project = read_manifest(current.project.directory, manifest.bytes);
  if (manifest_project.id != current.project.id || manifest_project.screenplay_file != current.project.screenplay_file ||
      parse_screenplay(screenplay.bytes) != current.screenplay) destination_changed(current.project.directory);
  // Strong exception guarantee: invalid candidates leave the old baseline intact.
  auto previous = opened_;
  auto old_manifest = manifest_, old_screenplay = screenplay_;
  opened_ = current;
  manifest_ = manifest;
  screenplay_ = screenplay;
  try { check_destination(); }
  catch (...) { opened_ = std::move(previous); manifest_ = std::move(old_manifest); screenplay_ = std::move(old_screenplay); throw; }
}
}
