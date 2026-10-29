#include "source/fountain_working.h"
#include "source/fountain_conversion.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <algorithm>

namespace apollo::source {
namespace {
[[noreturn]] void fail(const QString& reason, SourceError::Kind kind = SourceError::Kind::malformed) {
  throw SourceError(kind, "Editable Fountain copy: " + reason);
}
struct Review { ProjectAssets assets; QString contract; bool normalize_scene_spacing = false; };
Review review_of(const ProjectAssets& assets) {
  if (!assets.imported_material || !assets.original_fountain) fail("metadata and original are required");
  if (assets.imported_material->size() > maximum_fountain_metadata_bytes) fail("metadata exceeds its limit");
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(*assets.imported_material, &error);
  const auto root = json.object();
  if (root.value("version").toDouble() > 2) fail("needs a newer Apollo", SourceError::Kind::newer_version);
  const bool current = root.value("version") == 2 && root.value("contract") == writer_subset_contract;
  const bool legacy = root.value("version") == 1 &&
      (root.value("contract") == "writer-subset-1" || root.value("contract") == "writer-subset-2" ||
       root.value("contract") == "writer-subset-3" || root.value("contract") == "writer-subset-4");
  if (error.error != QJsonParseError::NoError || !json.isObject() || root.size() != (current ? 5 : 4) ||
      root.value("format") != "apollo-fountain-working" || (!current && !legacy) ||
      (current && !root.value("normalizeSceneSpacing").isBool()) ||
      !root.value("intake").isObject() || root.value("intake").toObject().value("version") != 2)
    fail("unsupported preservation contract");
  ProjectAssets review{QJsonDocument(root.value("intake").toObject()).toJson(), assets.original_fountain};
  validate_fountain_intake(review);
  return {review, root.value("contract").toString(), current && root.value("normalizeSceneSpacing").toBool()};
}
// Letters in a character name, before any extension, only appear in Fountain
// in lowercase when the cue is forced (@McCLANE): its author wants mixed case
// printed, an exception to presenting cues in capitals that is not specified.
bool mixed_case_name(const Element& e) {
  if (e.type != ElementType::cue) return false;
  const auto name = QString::fromUtf8(e.text).section('(', 0, 0);
  return std::any_of(name.begin(), name.end(), [](QChar c) { return c.isLower(); });
}
// writer-subset-1 admitted headings and cues only in ASCII capitals.
bool ascii_capitals(const Element& e) {
  return std::none_of(e.text.begin(), e.text.end(), [](char c) { return (c >= 'a' && c <= 'z') || uchar(c) >= 0x80; });
}
// writer-subset-3 spacing (docs/spacing.md). Fountain ignores spaces at the
// edges of a dialogue line and reads a line of spaces inside dialogue as a
// blank line; Apollo shows neither edge spaces nor more than a blank slot, so
// both keep their meaning. Repeated spaces inside action and dialogue lines
// are shown and read as written. Action indentation is meaningful in Fountain
// but not shown by Apollo, so it stays review-only, as does any other spacing.
bool admitted_spacing(const Element& e) {
  const auto lines = QString::fromUtf8(e.text).split('\n');
  if (e.type == ElementType::dialogue)
    return !lines.front().trimmed().isEmpty() && !lines.back().trimmed().isEmpty();
  if (e.type == ElementType::action)
    return std::none_of(lines.begin(), lines.end(), [](const QString& line) { return line.isEmpty() || line != line.trimmed(); });
  return false;
}
bool legacy_character(char32_t cp) {
  return (cp >= 32 && cp <= 126) || cp == '\n' || cp == 0xe9;
}
// Deliberately narrow: exactly two physically empty lines between ordinary
// action/dialogue and a scene heading, with no removed/retained material in the
// gap. All other vertical spacing remains gated, even after opting in.
std::vector<FountainRange> scene_spacing(const FountainConversion& c, const QByteArray& bytes) {
  std::vector<FountainRange> result;
  for (const auto& gap : c.limitations) {
    if (gap.code != "vertical-spacing") continue;
    const auto next = std::lower_bound(c.elements.begin(), c.elements.end(), gap.range.end,
        [](const LocatedFountainElement& e, qsizetype byte) { return e.range.start < byte; });
    if (next == c.elements.begin() || next == c.elements.end() || next->range.start != gap.range.end ||
        next->element.type != ElementType::heading) continue;
    const auto& previous = *std::prev(next);
    if (previous.element.type != ElementType::action && previous.element.type != ElementType::dialogue) continue;
    const auto raw = bytes.mid(previous.range.end, next->range.start - previous.range.end);
    if (raw == "\n\n\n" || raw == "\r\n\r\n\r\n" || raw == "\r\r\r") result.push_back(gap.range);
  }
  return result;
}
QStringList blockers(const FountainConversion& c, const QByteArray& bytes, const QString& contract, bool normalize_scene_spacing) {
  QStringList result;
  const bool current = contract == writer_subset_contract;
  const auto maximum_bytes = current ? writer_subset_maximum_bytes : 64 * 1024;
  const auto maximum_elements = current ? writer_subset_maximum_elements : 256;
  if (bytes.size() > maximum_bytes || c.elements.empty() || c.elements.size() > maximum_elements)
    result << QString("Writer subset is bounded to %1 KiB and 1–%2 elements.").arg(maximum_bytes / 1024).arg(maximum_elements);
  if (c.records.size() > 256) result << "Writer subset is bounded to 256 imported records.";
  if (bytes.contains('\t')) result << "Tabs are outside the verified writer subset.";
  if (std::any_of(c.unresolved.begin(), c.unresolved.end(), [&](const FountainIssue& issue) {
        return !current || issue.code != "empty-speech";
      })) result << "Unresolved source ranges require review.";
  const auto spacing = current ? scene_spacing(c, bytes) : std::vector<FountainRange>{};
  if (current && normalize_scene_spacing && spacing.empty())
    result << "No verified scene-spacing normalization is available for this source.";
  for (const auto& gap : c.limitations) {
    if (gap.code == "authored-case" || gap.code == "deliberate-break-layout") continue;
    const bool supported = gap.code == "vertical-spacing" &&
        std::find(spacing.begin(), spacing.end(), gap.range) != spacing.end();
    if (supported && normalize_scene_spacing) continue;
    result << (supported ? QString("Choose normal scene spacing explicitly to omit %1 extra empty lines before scene headings; original lines stay retained.").arg(spacing.size())
                         : "Unverified spacing or layout: " + gap.code);
  }
  for (const auto& located : c.elements) {
    const auto& e = located.element;
    const auto id = QString::fromStdString(e.id);
    const bool legacy_case = contract == "writer-subset-1" && (e.type == ElementType::heading || e.type == ElementType::cue) && !ascii_capitals(e);
    if (legacy_case) result << id + ": authored case requires review.";
    else if (mixed_case_name(e)) result << id + ": forced mixed-case character name requires review; Apollo presents cues in capitals.";
    // writer-subset-1 and -2 admitted no empty line and no leading, trailing or repeated space.
    const bool spacing = (contract == "writer-subset-3" || contract == "writer-subset-4" || current) && admitted_spacing(e);
    for (const auto gap : writer_subset_gaps(e, contract == "writer-subset-4" || current))
      if (!(legacy_case && gap == WriterSubsetGap::uncapitalized_letter) && !(spacing && gap == WriterSubsetGap::spacing)) switch (gap) {
        case WriterSubsetGap::character: result << id + ": character outside the verified writer subset."; break;
        case WriterSubsetGap::authored_case: result << id + ": authored case requires review."; break;
        case WriterSubsetGap::uncapitalized_letter: result << id + ": a letter that is not presented in capitals requires review."; break;
        case WriterSubsetGap::multiline: result << id + ": this element's multiline layout is unverified."; break;
        case WriterSubsetGap::long_title: result << id + ": long title field requires review."; break;
        case WriterSubsetGap::spacing: result << id + ": empty or significant spacing requires review."; break;
      }
  }
  if (result.isEmpty()) {
    try { (void)layout_document(c.screenplay().elements); }
    catch (const std::exception& error) { result << "Layout requires review: " + QString::fromUtf8(error.what()); }
  }
  result.removeDuplicates();
  return result;
}
// review_of has verified the complete frozen interpretation against these
// exact bytes. Do not validate it again through convert_fountain while
// deriving the working admission model.
FountainConversion working_conversion(const Review& review) {
  auto conversion = interpret_fountain(*review.assets.original_fountain);
  const auto reasons = blockers(conversion, *review.assets.original_fountain, review.contract, review.normalize_scene_spacing);
  if (!reasons.empty()) fail(reasons.join('\n'));
  return conversion;
}
}
bool writer_subset_character(char32_t cp) {
  return legacy_character(cp) || cp == 0x2014 || cp == 0x2018 || cp == 0x2019 ||
      cp == 0x201c || cp == 0x201d || cp == 0x2026;
}
std::vector<WriterSubsetGap> writer_subset_gaps(const Element& e, bool typographic_punctuation) {
  std::vector<WriterSubsetGap> result;
  const auto text = QString::fromUtf8(e.text);
  for (const auto cp : text.toUcs4())
    if (!(typographic_punctuation ? writer_subset_character(cp) : legacy_character(cp))) { result.push_back(WriterSubsetGap::character); break; }
  if (e.type == ElementType::title) {
    bool ascii_upper = true;
    for (const auto cp : text.toUcs4()) if (cp > 126 || (cp >= 'a' && cp <= 'z')) ascii_upper = false;
    if (!ascii_upper) result.push_back(WriterSubsetGap::authored_case);
  } else if (presented_in_capitals(e.type)) {
    const auto shown = QString::fromStdString(presented_text(e.type, e.text));
    if (std::any_of(shown.begin(), shown.end(), [](QChar c) { return c.isLower(); }))
      result.push_back(WriterSubsetGap::uncapitalized_letter);
  }
  if (text.contains('\n') && e.type != ElementType::action && e.type != ElementType::dialogue)
    result.push_back(WriterSubsetGap::multiline);
  if (is_title_field(e.type) && text.size() > 48) result.push_back(WriterSubsetGap::long_title);
  if (e.type != ElementType::page_break)
    for (const auto& line : text.split('\n'))
      if (line.isEmpty() || line != line.trimmed() || line.contains("  ")) { result.push_back(WriterSubsetGap::spacing); break; }
  return result;
}
bool is_fountain_working(const ProjectAssets& assets) {
  return assets.imported_material && QJsonDocument::fromJson(*assets.imported_material).object().value("format") == "apollo-fountain-working";
}
int fountain_scene_spacing_count(const ProjectAssets& review) {
  const auto c = convert_fountain(review);
  return int(scene_spacing(c, *review.original_fountain).size());
}
QStringList fountain_edit_blockers(const ProjectAssets& review, const QString& contract, bool normalize_scene_spacing) {
  const auto c = convert_fountain(review);
  return blockers(c, *review.original_fountain, contract, normalize_scene_spacing);
}
ProjectAssets editable_fountain_assets(const ProjectAssets& review, bool normalize_scene_spacing) {
  const auto interpreted = interpreted_fountain_assets(review);
  const auto reasons = fountain_edit_blockers(interpreted, writer_subset_contract, normalize_scene_spacing);
  if (!reasons.empty()) fail(reasons.join('\n'));
  const QJsonObject root{{"format", "apollo-fountain-working"}, {"version", 2}, {"contract", writer_subset_contract},
      {"normalizeSceneSpacing", normalize_scene_spacing},
      {"intake", QJsonDocument::fromJson(*interpreted.imported_material).object()}};
  return {QJsonDocument(root).toJson(), interpreted.original_fountain};
}
void validate_fountain_working(const ProjectAssets& assets) {
  // Validation is a pure function of the complete evidence bytes. Recovery
  // repeatedly opens copies of the same immutable assets on one worker.
  // Keep one exact-value result per thread, never a path/mtime/hash-only trust
  // cache. QByteArray's copy-on-write keeps this key immutable if a caller edits.
  thread_local std::optional<ProjectAssets> verified;
  if (verified && *verified == assets) return;
  (void)working_conversion(review_of(assets));
  verified = assets;
}
Screenplay initial_fountain_writing(const ProjectAssets& assets) {
  return working_conversion(review_of(assets)).screenplay();
}
FountainIntake open_working_fountain(const ProjectAssets& assets) {
  const auto review = review_of(assets);
  const auto c = working_conversion(review);
  FountainIntake result;
  result.interpreted = true;
  result.source_view = fountain_source_view(*review.assets.original_fountain);
  result.assets = assets;
  result.working = true;
  result.summary = QString("Editable import copy. %1 screenplay elements; %2 retained records.\n"
      "The original and imported records are read-only. Associations refer to original elements, not tracked text ranges; removed anchors are shown as Detached. "
      "Notes, stash, organization and production metadata stay outside screenplay output.\n%3")
      .arg(c.elements.size()).arg(c.records.size()).arg(imported_pdf_block);
  const auto spacing = scene_spacing(c, *review.assets.original_fountain);
  result.scene_spacing_count = int(spacing.size());
  if (!spacing.empty()) result.summary += QString("\nBy explicit import choice, %1 extra empty lines before scene headings use Apollo's normal scene spacing. "
      "They are retained below and in the original, not laid out or written in clean Fountain. To reverse this choice, return to the original review and make another copy.").arg(spacing.size());
  for (const auto& r : spacing)
    result.findings.append(QVariantMap{{"kind", "Scene spacing normalized by choice"}, {"startByte", r.start}, {"endByte", r.end},
      {"firstLine", r.first_line}, {"lastLine", r.last_line}, {"text", ""},
      {"truncated", false}, {"message", "One extra empty line before a scene heading is retained in the original, not laid out or exported; normal scene spacing was explicitly chosen."},
      {"anchor", ""}, {"anchorType", -1}});
  for (const auto& issue : c.unresolved)
    result.findings.append(QVariantMap{{"kind", "Original diagnostic: " + issue.code}, {"startByte", issue.range.start}, {"endByte", issue.range.end},
      {"firstLine", issue.range.first_line}, {"lastLine", issue.range.last_line}, {"text", ""},
      {"truncated", false}, {"message", issue.message + " Current writing diagnostics appear in the Writing desk; original evidence is unchanged."},
      {"anchor", ""}, {"anchorType", -1}});
  QMap<QString, int> types;
  for (const auto& e : c.elements) types.insert(QString::fromStdString(e.element.id), int(e.element.type));
  for (const auto& r : c.records)
    result.findings.append(QVariantMap{{"kind", r.name}, {"startByte", r.range.start}, {"endByte", r.range.end},
      {"firstLine", r.range.first_line}, {"lastLine", r.range.last_line}, {"text", r.text}, {"truncated", false},
      {"message", "Retained " + r.kind + "; original association: " + r.anchor},
      {"anchor", r.anchor}, {"anchorType", types.value(r.anchor, -1)}});
  return result;
}
QString fountain_export_omissions(const ProjectAssets& assets) {
  if (!is_fountain_working(assets)) return {};
  const auto review = review_of(assets);
  const auto conversion = interpret_fountain(*review.assets.original_fountain);
  const auto& records = conversion.records;
  const std::pair<const char*, std::pair<const char*, const char*>> labels[] = {
      {"note", {"imported note", "imported notes"}}, {"stash", {"stash passage", "stash passages"}},
      {"titleMetadata", {"unsupported title-page field", "unsupported title-page fields"}},
      {"section", {"section", "sections"}}, {"synopsis", {"synopsis", "synopses"}},
      {"sceneNumber", {"production scene number", "production scene numbers"}},
      {"spacing", {"whitespace-only source line", "whitespace-only source lines"}}};
  QStringList parts;
  for (const auto& [kind, names] : labels) {
    const auto count = std::count_if(records.begin(), records.end(), [&](const FountainRecord& r) { return r.kind == kind; });
    if (count) parts << QString("%1 %2").arg(count).arg(count == 1 ? names.first : names.second);
  }
  QString disclosure;
  if (!parts.isEmpty()) disclosure = "Not included in clean Fountain: " + parts.join(", ") + ". They stay in this project's imported material and original.";
  if (review.normalize_scene_spacing) {
    if (!disclosure.isEmpty()) disclosure += '\n';
    disclosure += QString("By explicit import choice, %1 extra empty lines before scene headings use Apollo's normal scene spacing and are not written. "
        "Their exact source locations and original lines stay in the imported material; this is not spacing-equivalent output.")
        .arg(scene_spacing(conversion, *review.assets.original_fountain).size());
  }
  return disclosure;
}
QString imported_origin(const ProjectAssets& assets) {
  if (!assets.imported_material) return {};
  const auto root = QJsonDocument::fromJson(*assets.imported_material).object();
  return (is_fountain_working(assets) ? root.value("intake").toObject() : root).value("origin").toString();
}
}
