#include "source/fountain_import.h"
#include "source/fountain_conversion.h"
#include "source/project_creation.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <algorithm>

namespace apollo::source {
namespace {
[[noreturn]] void refuse(const QString& message, SourceError::Kind kind = SourceError::Kind::malformed) {
  throw SourceError(kind, message);
}
QString counted(qsizetype count, const char* one, const char* many) {
  return QStringLiteral("%1 %2").arg(count).arg(QString::fromLatin1(count == 1 ? one : many));
}
QByteArray read_fountain(const QString& path) {
  const QFileInfo info(path);
  if (path.trimmed().isEmpty()) refuse(QStringLiteral("Choose a Fountain file to import."));
  if (!info.exists()) refuse(QStringLiteral("%1 does not exist.").arg(path), SourceError::Kind::io);
  if (!info.isFile()) refuse(QStringLiteral("%1 is not a file.").arg(path), SourceError::Kind::io);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) refuse(QStringLiteral("Cannot read %1: %2").arg(path, file.errorString()), SourceError::Kind::io);
  return file.readAll();
}
ImportedProject publish(const QString& fountain_path, const std::function<Project(Screenplay)>& create) {
  auto imported = import_fountain_text(read_fountain(fountain_path));
  return {create(std::move(imported.screenplay)), std::move(imported.notice)};
}
}

FountainImport import_fountain_text(const QByteArray& bytes) {
  const auto conversion = interpret_fountain(bytes);
  for (const auto& issue : conversion.unresolved) {
    if (issue.code == "encoding")
      refuse(QStringLiteral("This file is not UTF-8 text. Save it as UTF-8 and import it again."));
    if (issue.code == "conversion-limit")
      refuse(QStringLiteral("This file is larger than Apollo can import (1 MiB or 100,000 lines)."));
  }

  FountainImport result;
  // Fresh identities: this is an ordinary project, not a tracked import.
  // Continuation lines of one title key arrive as separate entries; keep them
  // together as one multiline field, as they were written.
  for (const auto& located : conversion.elements) {
    auto element = located.element;
    element.id.clear();
    auto& elements = result.screenplay.elements;
    if (is_title_field(element.type) && !elements.empty() && elements.back().type == element.type) {
      auto& previous = elements.back();
      const auto offset = previous.text.size() + 1;
      previous.text += '\n' + element.text;
      for (auto span : element.styles) { span.start += offset; span.end += offset; previous.styles.push_back(span); }
      continue;
    }
    elements.push_back(std::move(element));
  }
  const bool has_body = std::any_of(result.screenplay.elements.begin(), result.screenplay.elements.end(),
                                    [](const Element& e) { return !is_title_field(e.type); });
  if (!has_body) result.screenplay.elements.push_back({ElementType::action, ""});

  const auto records = [&](const char* kind) {
    return std::count_if(conversion.records.begin(), conversion.records.end(),
                         [&](const FountainRecord& r) { return r.kind == QLatin1String(kind); });
  };
  const auto unclosed = std::count_if(conversion.unresolved.begin(), conversion.unresolved.end(), [](const FountainIssue& i) {
    return i.code == "unclosed-delimiter" || i.code == "nested-delimiter";
  });
  QStringList left_out;
  if (const auto n = records("note")) left_out << counted(n, "note", "notes");
  if (const auto n = records("stash")) left_out << counted(n, "boneyard passage", "boneyard passages");
  if (const auto n = records("section")) left_out << counted(n, "section heading", "section headings");
  if (const auto n = records("synopsis")) left_out << counted(n, "synopsis", "synopses");
  if (const auto n = records("sceneNumber")) left_out << counted(n, "scene number", "scene numbers");
  if (const auto n = records("titleMetadata")) left_out << counted(n, "other title-page field", "other title-page fields");
  if (unclosed) left_out << counted(unclosed, "passage with an unmatched [[ or /*", "passages with an unmatched [[ or /*");
  if (!left_out.isEmpty())
    result.notice = QStringLiteral("Not imported (Apollo has no place for these yet): %1. Your Fountain file is unchanged.")
                        .arg(left_out.join(QStringLiteral(", ")));
  return result;
}

ImportedProject import_fountain_project(const QString& parent, const QString& name, const QString& fountain_path) {
  if (name.trimmed().isEmpty()) refuse(QStringLiteral("Name the new project."));
  return publish(fountain_path, [&](Screenplay screenplay) { return create_named_project(parent, name, std::move(screenplay), {}); });
}
ImportedProject import_fountain_project_at(const QString& destination, const QString& fountain_path) {
  return publish(fountain_path, [&](Screenplay screenplay) { return create_project(destination, std::move(screenplay)); });
}
}
