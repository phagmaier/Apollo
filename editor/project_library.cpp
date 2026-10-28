#include "editor/project_library.h"
#include "source/project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace apollo::editor {
namespace {
constexpr int maximum_projects = 20;

[[noreturn]] void fail(source::SourceError::Kind kind, const QString& path, const QString& reason) {
  throw source::SourceError(kind, QStringLiteral("recent projects %1: %2").arg(path, reason));
}

bool valid_path(const QString& path) {
  return !path.isEmpty() && path.isValidUtf16() && !path.contains(QChar::Null) &&
         QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path;
}

QString canonical_directory(const QString& directory) {
  const auto canonical = QFileInfo(directory).canonicalFilePath();
  return canonical.isEmpty() ? QDir::cleanPath(QFileInfo(directory).absoluteFilePath()) : canonical;
}

void validate(const RecentProject& project, const QString& path) {
  if (!valid_path(project.directory))
    fail(source::SourceError::Kind::malformed, path, QStringLiteral("directory must be a clean absolute path"));
  const auto uuid = QUuid::fromString(project.project_id);
  if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != project.project_id)
    fail(source::SourceError::Kind::malformed, path, QStringLiteral("projectId must be a canonical lowercase UUID"));
}

bool same_project(const RecentProject& left, const RecentProject& right) {
  return left.project_id == right.project_id || left.directory == right.directory;
}
}

ProjectLibrary::ProjectLibrary(QString path) : path_(std::move(path)) {
  if (path_.isEmpty()) {
    const auto root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (root.isEmpty()) fail(source::SourceError::Kind::io, QStringLiteral("Apollo/recent-projects.json"), QStringLiteral("no application data location"));
    path_ = root + QStringLiteral("/Apollo/recent-projects.json");
  }
  if (!valid_path(path_))
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("list file must have a clean absolute path"));
}

std::vector<RecentProject> ProjectLibrary::load() const {
  // Do not follow a symlink, or block on a FIFO, even if a path changes while
  // being opened. Validate the opened file rather than a previous path stat.
  const int fd = ::open(QFile::encodeName(path_).constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) {
    const int error = errno;
    if (error == ENOENT) return {};
    fail(source::SourceError::Kind::io, path_, QString::fromLocal8Bit(std::strerror(error)));
  }
  QFile file;
  if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(fd);
    fail(source::SourceError::Kind::io, path_, file.errorString());
  }
  struct stat status {};
  if (::fstat(fd, &status) != 0)
    fail(source::SourceError::Kind::io, path_, QString::fromLocal8Bit(std::strerror(errno)));
  if (!S_ISREG(status.st_mode))
    fail(source::SourceError::Kind::io, path_, QStringLiteral("must be a regular file, not a symlink or directory"));
  const auto bytes = file.readAll();
  if (file.error() != QFileDevice::NoError) fail(source::SourceError::Kind::io, path_, file.errorString());
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !json.isObject())
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("invalid JSON object: %1").arg(error.errorString()));
  const auto root = json.object();
  const auto version = root.value("version");
  if (version.isDouble() && version.toDouble() > 1)
    fail(source::SourceError::Kind::newer_version, path_, QStringLiteral("version %1 needs a newer Apollo").arg(version.toDouble()));
  if (!version.isDouble() || version.toDouble() != 1)
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("unsupported version"));
  if (root.size() != 3 || root.value("format").toString() != QLatin1String("apollo-recent-projects") || !root.value("projects").isArray())
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("missing, unexpected, or invalid list fields"));
  const auto projects = root.value("projects").toArray();
  if (projects.size() > maximum_projects)
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("list exceeds 20 projects"));
  std::vector<RecentProject> result;
  result.reserve(projects.size());
  for (const auto& entry : projects) {
    const auto object = entry.toObject();
    if (!entry.isObject() || object.size() != 2 || !object.value("directory").isString() || !object.value("projectId").isString())
      fail(source::SourceError::Kind::malformed, path_, QStringLiteral("invalid project entry fields"));
    RecentProject project{object.value("directory").toString(), object.value("projectId").toString()};
    validate(project, path_);
    project.directory = canonical_directory(project.directory);
    // Existing paths can become aliases after the list was written. Prefer
    // the first (most recently activated) entry without touching the file.
    if (std::none_of(result.begin(), result.end(), [&](const auto& prior) { return same_project(prior, project); }))
      result.push_back(std::move(project));
  }
  return result;
}

void ProjectLibrary::record(const RecentProject& project) const {
  if (project.directory.isEmpty() || !project.directory.isValidUtf16() || project.directory.contains(QChar::Null))
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("invalid project directory"));
  RecentProject activated{canonical_directory(project.directory), QUuid::fromString(project.project_id).toString(QUuid::WithoutBraces)};
  validate(activated, path_);
  if (!QDir().mkpath(QFileInfo(path_).absolutePath()))
    fail(source::SourceError::Kind::io, path_, QStringLiteral("cannot create application data directory"));
  QLockFile lock(path_ + QStringLiteral(".lock"));
  lock.setStaleLockTime(0);
  if (!lock.tryLock(0))
    fail(lock.error() == QLockFile::LockFailedError ? source::SourceError::Kind::locked : source::SourceError::Kind::io,
         path_, QStringLiteral("cannot lock recent projects list"));
  // Never merge a cached pre-lock snapshot or overwrite changed evidence.
  auto projects = load();
  std::erase_if(projects, [&](const auto& prior) { return same_project(prior, activated); });
  projects.insert(projects.begin(), std::move(activated));
  if (projects.size() > maximum_projects) projects.resize(maximum_projects);
  QJsonArray entries;
  for (const auto& item : projects)
    entries.append(QJsonObject{{"directory", item.directory}, {"projectId", item.project_id}});
  source::replace_file(path_, QJsonDocument(QJsonObject{{"format", "apollo-recent-projects"}, {"version", 1}, {"projects", entries}}).toJson());
}
}
