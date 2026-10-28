#include "source/project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>

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
Project read_manifest(const QString& directory) {
  const auto path = directory + QLatin1Char('/') + manifest_name;
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(read_all(path), &error);
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
  auto project = read_manifest(absolute);
  auto screenplay = parse_screenplay(read_all(project.screenplay_path()));
  return {std::move(project), std::move(screenplay)};
}

Project create_project(const QString& destination, Screenplay screenplay) {
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
  // Publish only what reads back as a complete project.
  if (!(open_project(staging.path()).screenplay == canonical(screenplay))) malformed(QStringLiteral("the staged project did not read back"));
  // QTemporaryDir is private to its owner; give the project ordinary folder access.
  QFile::setPermissions(staging.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup |
                                        QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
  if (::renameat2(AT_FDCWD, QFile::encodeName(staging.path()).constData(), AT_FDCWD, QFile::encodeName(path).constData(),
                  RENAME_NOREPLACE) != 0)
    io_error(QStringLiteral("cannot create %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno))));
  staging.setAutoRemove(false);
  return {path, id, default_screenplay};
}
}
