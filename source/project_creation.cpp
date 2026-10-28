#include "source/project_creation.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace apollo::source {
namespace {
[[noreturn]] void invalid(const QString& message) {
  throw SourceError(SourceError::Kind::malformed, message);
}
[[noreturn]] void io_error(const QString& message) {
  throw SourceError(SourceError::Kind::io, message);
}

QString project_name(const QString& input) {
  if (!input.isValidUtf16()) invalid(QStringLiteral("project name contains invalid Unicode"));
  for (const auto character : input) {
    if (character == QLatin1Char('/') || character == QLatin1Char('\\') || character.category() == QChar::Other_Control)
      invalid(QStringLiteral("project name must be one folder name without separators or control characters"));
  }
  const auto name = input.trimmed();
  if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
    invalid(QStringLiteral("project name must be a nonempty folder name other than . or .."));
  return name;
}
}

QString default_project_location() {
  auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  if (documents.isEmpty()) documents = QDir::homePath();
  return QDir::cleanPath(documents + QStringLiteral("/Apollo"));
}

Project create_blank_project(const QString& parent, const QString& input_name) {
  return create_named_project(parent, input_name, {0, {{ElementType::action, ""}}}, {});
}

Project create_named_project(const QString& parent, const QString& input_name, Screenplay screenplay, const ProjectAssets& assets) {
  const auto name = project_name(input_name);
  if (!parent.isValidUtf16() || parent.contains(QChar(0)) || !QDir::isAbsolutePath(parent) || QDir::cleanPath(parent) != parent)
    invalid(QStringLiteral("project parent must be a clean absolute folder path"));

  const QFileInfo parent_info(parent);
  if (!parent_info.exists() && !parent_info.isSymLink()) {
    if (parent != default_project_location()) io_error(QStringLiteral("project parent %1 does not exist").arg(parent));
    if (!QDir().mkpath(parent)) io_error(QStringLiteral("cannot create default project folder %1").arg(parent));
  }
  const QFileInfo existing_parent(parent);
  const auto canonical_parent = existing_parent.canonicalFilePath();
  if (!existing_parent.isDir() || canonical_parent.isEmpty()) io_error(QStringLiteral("project parent %1 is not an accessible folder").arg(parent));

  constexpr int maximum_candidates = 1000;
  for (int candidate = 1; candidate <= maximum_candidates; ++candidate) {
    const auto folder = candidate == 1 ? name : QStringLiteral("%1 (%2)").arg(name).arg(candidate);
    const auto destination = QDir(canonical_parent).filePath(folder);
    const QFileInfo target(destination);
    if (target.exists() || target.isSymLink()) continue;
    // create_project owns staging, validation, and no-replace publication.
    // Do not turn an arbitrary I/O error into a collision retry: a competing
    // publish after this check fails safely, leaving that destination intact.
    return create_project(destination, std::move(screenplay), assets);
  }
  io_error(QStringLiteral("cannot create project %1: all %2 folder names already exist in %3")
               .arg(name).arg(maximum_candidates).arg(canonical_parent));
}
}
