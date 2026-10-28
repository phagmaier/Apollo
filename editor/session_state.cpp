#include "editor/session_state.h"
#include "editor/screenplay_layout.h"
#include "source/project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocument>
#include <QUuid>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <limits>
#include <sys/stat.h>
#include <utility>

namespace apollo::editor {
namespace {
[[noreturn]] void fail(source::SourceError::Kind kind, const QString& path, const QString& reason) {
  throw source::SourceError(kind, QStringLiteral("session state %1: %2").arg(path, reason));
}

bool valid_path(const QString& path) {
  return !path.isEmpty() && path.isValidUtf16() && !path.contains(QChar::Null) &&
         QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path;
}

void validate(const ResumeState& state, const QString& path) {
  const auto malformed = [&](const QString& reason) { fail(source::SourceError::Kind::malformed, path, reason); };
  if (!valid_path(state.directory)) malformed(QStringLiteral("directory must be a clean absolute path"));
  const auto uuid = QUuid::fromString(state.project_id);
  if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != state.project_id)
    malformed(QStringLiteral("projectId must be a canonical lowercase UUID"));
  if (state.element_id.size() > 64 || !std::all_of(state.element_id.begin(), state.element_id.end(), [](QChar c) {
        const auto value = c.unicode();
        return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
               (value >= '0' && value <= '9') || value == '_' || value == '-';
      })) malformed(QStringLiteral("elementId must be empty or 1–64 ASCII letters, digits, underscores or hyphens"));
  if (state.offset < 0 || state.position < 0) malformed(QStringLiteral("cursor coordinates must be nonnegative integers"));
}

int coordinate(const QJsonValue& value, const QString& path, const QString& field) {
  const double number = value.toDouble(-1);
  if (!value.isDouble() || !std::isfinite(number) || number < 0 ||
      number > std::numeric_limits<int>::max() || std::floor(number) != number)
    fail(source::SourceError::Kind::malformed, path, field + QStringLiteral(" must be a nonnegative integer within Qt's cursor range"));
  return static_cast<int>(number);
}

int bounded_position(const QTextDocument& document, int position) {
  position = std::clamp(position, 0, document.characterCount() - 1);
  if (position > 0 && document.characterAt(position).isLowSurrogate() &&
      document.characterAt(position - 1).isHighSurrogate()) --position;
  return position;
}
}

SessionState::SessionState(QString path) : path_(std::move(path)) {
  if (path_.isEmpty()) {
    const auto root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (root.isEmpty()) fail(source::SourceError::Kind::io, QStringLiteral("Apollo/session.json"), QStringLiteral("no application data location"));
    path_ = root + QStringLiteral("/Apollo/session.json");
  }
  if (!valid_path(path_)) fail(source::SourceError::Kind::malformed, path_, QStringLiteral("state file must have a clean absolute path"));
}

std::optional<ResumeState> SessionState::load() const {
  // Distinguish absence from inaccessible paths, and never follow a symlink
  // that might make unrelated data appear safe to replace.
  struct stat status {};
  if (::lstat(QFile::encodeName(path_).constData(), &status) != 0) {
    const int error = errno;
    if (error == ENOENT) return std::nullopt;
    fail(source::SourceError::Kind::io, path_, QString::fromLocal8Bit(std::strerror(error)));
  }
  if (!S_ISREG(status.st_mode)) fail(source::SourceError::Kind::io, path_, QStringLiteral("must be a regular file, not a symlink or directory"));
  QFile file(path_);
  if (!file.open(QIODevice::ReadOnly)) fail(source::SourceError::Kind::io, path_, file.errorString());
  const auto bytes = file.readAll();
  if (file.error() != QFileDevice::NoError) fail(source::SourceError::Kind::io, path_, file.errorString());
  QJsonParseError parse_error;
  const auto json = QJsonDocument::fromJson(bytes, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !json.isObject())
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("invalid JSON object: %1").arg(parse_error.errorString()));
  const auto root = json.object();
  const auto version = root.value("version");
  if (version.isDouble() && version.toDouble() > 1)
    fail(source::SourceError::Kind::newer_version, path_, QStringLiteral("version %1 needs a newer Apollo").arg(version.toDouble()));
  if (!version.isDouble() || version.toDouble() != 1)
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("unsupported version"));
  if (root.size() != 7)
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("missing or unexpected fields"));
  for (const char* field : {"format", "directory", "projectId", "elementId"}) {
    if (!root.value(QLatin1String(field)).isString())
      fail(source::SourceError::Kind::malformed, path_, QStringLiteral("%1 must be a string").arg(QLatin1String(field)));
  }
  if (root.value("format").toString() != QLatin1String("apollo-session"))
    fail(source::SourceError::Kind::malformed, path_, QStringLiteral("not an Apollo session file"));
  ResumeState state{root.value("directory").toString(), root.value("projectId").toString(), root.value("elementId").toString(),
                    coordinate(root.value("offset"), path_, QStringLiteral("offset")),
                    coordinate(root.value("position"), path_, QStringLiteral("position"))};
  validate(state, path_);
  return state;
}

void SessionState::save(const ResumeState& state) const {
  validate(state, path_);
  // Invalid/newer evidence must survive even when another project activates.
  load();
  if (!QDir().mkpath(QFileInfo(path_).absolutePath()))
    fail(source::SourceError::Kind::io, path_, QStringLiteral("cannot create application state directory"));
  const QJsonObject json{{"format", "apollo-session"}, {"version", 1}, {"directory", state.directory},
                         {"projectId", state.project_id}, {"elementId", state.element_id},
                         {"offset", state.offset}, {"position", state.position}};
  source::replace_file(path_, QJsonDocument(json).toJson());
}

ResumeState SessionState::capture(const QString& directory, const QString& project_id,
                                 const QTextDocument& document, int cursor) {
  cursor = bounded_position(document, cursor);
  const auto block = document.findBlock(cursor);
  return {directory, project_id, block.blockFormat().stringProperty(element_id_property), cursor - block.position(), cursor};
}

int SessionState::cursor_position(const ResumeState& state, const QTextDocument& document) {
  if (!state.element_id.isEmpty()) {
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
      if (block.blockFormat().stringProperty(element_id_property) == state.element_id) {
        const int offset = std::clamp(state.offset, 0, block.length() - 1);
        return bounded_position(document, block.position() + offset);
      }
    }
  }
  return bounded_position(document, state.position);
}
}
