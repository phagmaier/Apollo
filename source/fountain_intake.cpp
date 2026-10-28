#include "source/fountain_intake.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <vector>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace apollo::source {
namespace {
[[noreturn]] void fail(const QString& reason, SourceError::Kind kind = SourceError::Kind::malformed) {
  throw SourceError(kind, QStringLiteral("Fountain intake: %1").arg(reason));
}
QString digest(const QByteArray& bytes) {
  return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString display(const QByteArray& bytes) {
  const auto text = QString::fromUtf8(bytes);
  QString result;
  for (const auto c : text) {
    if ((c.category() == QChar::Other_Control && c != '\n' && c != '\r' && c != '\t') ||
        c.category() == QChar::Other_Format || c == QChar(0x2028) || c == QChar(0x2029))
      result += QStringLiteral("\\u%1").arg(static_cast<unsigned>(c.unicode()), 4, 16, QLatin1Char('0'));
    else result += c;
  }
  return result;
}
FountainIntake describe(ProjectAssets assets, const QString& origin) {
  FountainIntake result;
  result.assets = std::move(assets);
  const auto& bytes = *result.assets.original_fountain;
  // Build the line map once; findings keep original byte offsets, including
  // BOM and CRLF, and never scan the complete prefix for every finding.
  std::vector<qsizetype> line_starts{0};
  for (qsizetype i = 0; i < bytes.size(); ++i) {
    if (bytes[i] == '\r') { if (i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i; line_starts.push_back(i + 1); }
    else if (bytes[i] == '\n') line_starts.push_back(i + 1);
  }
  const auto line_at = [&](qsizetype offset) {
    return static_cast<int>(std::upper_bound(line_starts.begin(), line_starts.end(), offset) - line_starts.begin());
  };
  static const QRegularExpression page_break("^={3,}$"), metadata("^[A-Za-z][A-Za-z ]*:"), scene_number("#[A-Za-z0-9.-]+#\\s*$");
  auto body = bytes;
  const bool bom = body.startsWith("\xef\xbb\xbf");
  if (bom) body.remove(0, 3);
  const bool utf8 = QString::fromUtf8(body).toUtf8() == body;
  result.summary = QStringLiteral("%1 bytes captured from %2. Original bytes verified.\n"
      "Review only: the entire source is pending screenplay conversion. Editing and screenplay exports are unavailable. "
      "Findings are hints, not a complete syntax interpretation. Notes and omitted passages have not yet been converted.\n%3")
      .arg(bytes.size()).arg(display(origin.toUtf8()), utf8
        ? QStringLiteral("UTF-8 reference view; control/format characters are shown as \\u escapes. Original bytes, line endings and any BOM are retained.")
        : QStringLiteral("Invalid UTF-8 or another encoding: hexadecimal byte view. No replacement characters or encoding guesses were saved."));
  if (utf8) result.source_view = display(body);
  else {
    for (qsizetype i = 0; i < bytes.size(); i += 32)
      result.source_view += QStringLiteral("%1  %2\n").arg(i, 8, 16, QLatin1Char('0')).arg(QString::fromLatin1(bytes.mid(i, 32).toHex(' ')));
  }
  const auto add = [&](const QString& kind, qsizetype start, qsizetype end, const QString& message) {
    if (result.findings.size() >= 200) return;
    auto preview_end = qMin(end, start + 2000);
    if (utf8) while (preview_end < end && (static_cast<unsigned char>(bytes[preview_end]) & 0xc0) == 0x80) --preview_end;
    result.findings.push_back(QVariantMap{{"kind", kind}, {"startByte", start}, {"endByte", end},
      {"firstLine", line_at(start)}, {"lastLine", line_at(qMax(start, end - 1))},
      {"message", message}, {"truncated", preview_end < end}, {"text", utf8 ? display(bytes.mid(start, preview_end - start)) : QString()}});
  };
  if (!utf8) add("encoding", 0, bytes.size(), "Encoding needs explicit review; all bytes are retained.");
  // Delimiter candidates. Do not pretend to resolve nesting, escapes, note
  // paragraph boundaries or interactions with other syntax in this first slice.
  for (qsizetype i = 0; i < bytes.size() && result.findings.size() < 200;) {
    const bool note = i + 1 < bytes.size() && bytes[i] == '[' && bytes[i + 1] == '[';
    const bool stash = i + 1 < bytes.size() && bytes[i] == '/' && bytes[i + 1] == '*';
    if (!note && !stash) { ++i; continue; }
    const auto closing = bytes.indexOf(note ? "]]" : "*/", i + 2);
    const auto end = closing < 0 ? bytes.size() : closing + 2;
    add(note ? "note candidate" : "omitted passage candidate", i, end,
        closing < 0 ? "Unclosed delimiter; interpretation is unresolved." : "Retained for review; delimiter nesting/escaping and conversion remain unresolved.");
    i = end;
  }
  // A bounded line inventory supplements the complete source view. It is not
  // used to remove or publish any screenplay content.
  int lines = 0;
  for (qsizetype start = 0; start < bytes.size() && result.findings.size() < 200; ++lines) {
    auto end = start;
    while (end < bytes.size() && bytes[end] != '\r' && bytes[end] != '\n') ++end;
    const auto line = QString::fromUtf8(bytes.mid(start, end - start));
    const auto trimmed = line.trimmed();
    if (trimmed.startsWith('#')) add("section candidate", start, end, "Organization retained; not a printed act label.");
    else if (trimmed.startsWith('=') && !page_break.match(trimmed).hasMatch())
      add("synopsis candidate", start, end, "Organizational information retained outside screenplay conversion.");
    else if (lines == 0 || (lines < 100 && !line.isEmpty())) {
      if (metadata.match(line).hasMatch())
        add("metadata or text candidate", start, end, "Could be title metadata or screenplay text; review in context.");
    }
    if (scene_number.match(line).hasMatch())
      add("scene number candidate", start, end, "Production numbering is retained; editing/exports remain pending.");
    if (trimmed.startsWith('@') || trimmed.startsWith('!') || trimmed.startsWith('.') || trimmed.endsWith('^'))
      add("forced element or dual candidate", start, end, "Element interpretation, case and dialogue relationships need conversion checks.");
    if (line.startsWith(' ') || line.startsWith('\t') || line.endsWith(' ') || line.endsWith('\t'))
      add("whitespace", start, end, "Intentional spacing or element-sensitive whitespace must be preserved by conversion.");
    if (trimmed.contains('*') || trimmed.contains('_') || trimmed.contains('\\'))
      add("emphasis or literal candidate", start, end, "Literal/escaped punctuation and emphasis need interpretation in context.");
    start = end;
    if (start < bytes.size() && bytes[start++] == '\r' && start < bytes.size() && bytes[start] == '\n') ++start;
  }
  if (result.findings.size() == 200) result.summary += "\nFinding list capped at 200; the complete source remains available.";
  return result;
}
}

bool is_fountain_intake(const ProjectAssets& assets) {
  return assets.imported_material && QJsonDocument::fromJson(*assets.imported_material).object().value("format") == "apollo-fountain-intake";
}

void validate_fountain_intake(const ProjectAssets& assets) {
  if (!assets.imported_material || !assets.original_fountain) fail("metadata and original source are both required");
  if (assets.original_fountain->size() > maximum_fountain_bytes) fail("source exceeds the 16 MiB intake limit");
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(*assets.imported_material, &error);
  const auto root = json.object();
  if (error.error != QJsonParseError::NoError || !json.isObject() || root.value("format") != "apollo-fountain-intake") fail("invalid metadata format");
  if (root.value("version").toDouble() > 1) fail("metadata needs a newer Apollo", SourceError::Kind::newer_version);
  if (root.size() != 6 || root.value("version") != 1 || root.value("status") != "pending-conversion" ||
      !root.value("origin").isString() || root.value("origin").toString().isEmpty() || !root.value("origin").toString().isValidUtf16() ||
      !QDir::isAbsolutePath(root.value("origin").toString()) || root.value("origin").toString().contains(QChar::Null) ||
      !root.value("sha256").isString() || root.value("sha256").toString() != digest(*assets.original_fountain) ||
      !root.value("bytes").isDouble() || root.value("bytes").toDouble() != assets.original_fountain->size()) fail("metadata or original source does not verify");
}

FountainIntake open_fountain_intake(const ProjectAssets& assets) {
  validate_fountain_intake(assets);
  const auto origin = QJsonDocument::fromJson(*assets.imported_material).object().value("origin").toString();
  return describe(assets, origin);
}

FountainIntake read_fountain_intake(const QString& path) {
  if (path.isEmpty() || !path.isValidUtf16() || path.contains(QChar::Null)) fail("choose a local source file");
  // Open once, nonblocking, then validate the descriptor. Source symlinks are
  // refused explicitly; parent-folder aliases are harmless for read-only input.
  const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) fail(QStringLiteral("cannot read %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno))), SourceError::Kind::io);
  QFile file;
  if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) { ::close(fd); fail("cannot hold input file", SourceError::Kind::io); }
  struct stat before {}, after {};
  if (::fstat(fd, &before) != 0 || !S_ISREG(before.st_mode)) fail("input must be a regular file", SourceError::Kind::io);
  if (before.st_size > maximum_fountain_bytes) fail("source exceeds the 16 MiB intake limit");
  const auto bytes = file.read(maximum_fountain_bytes + 1);
  if (file.error() != QFileDevice::NoError) fail(file.errorString(), SourceError::Kind::io);
  if (bytes.size() > maximum_fountain_bytes || ::fstat(fd, &after) != 0 || before.st_dev != after.st_dev ||
      before.st_ino != after.st_ino || before.st_size != after.st_size || bytes.size() != after.st_size ||
      before.st_mtim.tv_sec != after.st_mtim.tv_sec || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
      before.st_ctim.tv_sec != after.st_ctim.tv_sec || before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)
    fail("source changed while reading; preview again", SourceError::Kind::external_change);
  const auto origin = QFileInfo(path).absoluteFilePath();
  ProjectAssets assets{QJsonDocument(QJsonObject{{"format", "apollo-fountain-intake"}, {"version", 1},
      {"status", "pending-conversion"}, {"origin", origin}, {"sha256", digest(bytes)}, {"bytes", bytes.size()}}).toJson(), bytes};
  return open_fountain_intake(assets);
}
}
