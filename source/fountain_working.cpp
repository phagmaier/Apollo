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
struct Review { ProjectAssets assets; QString contract; };
Review review_of(const ProjectAssets& assets) {
  if (!assets.imported_material || !assets.original_fountain) fail("metadata and original are required");
  if (assets.imported_material->size() > maximum_fountain_metadata_bytes) fail("metadata exceeds its limit");
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(*assets.imported_material, &error);
  const auto root = json.object();
  if (root.value("version").toDouble() > 1) fail("needs a newer Apollo", SourceError::Kind::newer_version);
  if (error.error != QJsonParseError::NoError || !json.isObject() || root.size() != 4 ||
      root.value("format") != "apollo-fountain-working" || root.value("version") != 1 ||
      (root.value("contract") != "writer-subset-1" && root.value("contract") != "writer-subset-2" &&
       root.value("contract") != "writer-subset-3" && root.value("contract") != writer_subset_contract) ||
      !root.value("intake").isObject() || root.value("intake").toObject().value("version") != 2)
    fail("unsupported preservation contract");
  ProjectAssets review{QJsonDocument(root.value("intake").toObject()).toJson(), assets.original_fountain};
  validate_fountain_intake(review);
  return {review, root.value("contract").toString()};
}
ProjectAssets review_assets(const ProjectAssets& assets) { return review_of(assets).assets; }
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
QStringList blockers(const FountainConversion& c, qsizetype size, const QString& contract) {
  QStringList result;
  if (size > 64 * 1024 || c.elements.empty() || c.elements.size() > 256) result << "Writer subset is bounded to 64 KiB and 1–256 elements.";
  if (c.records.size() > 256) result << "Writer subset is bounded to 256 imported records.";
  if (!c.unresolved.empty()) result << "Unresolved source ranges require review.";
  for (const auto& gap : c.limitations)
    if (gap.code != "authored-case" && gap.code != "deliberate-break-layout")
      result << "Unverified spacing or layout: " + gap.code;
  for (const auto& located : c.elements) {
    const auto& e = located.element;
    const auto id = QString::fromStdString(e.id);
    const bool legacy_case = contract == "writer-subset-1" && (e.type == ElementType::heading || e.type == ElementType::cue) && !ascii_capitals(e);
    if (legacy_case) result << id + ": authored case requires review.";
    else if (mixed_case_name(e)) result << id + ": forced mixed-case character name requires review; Apollo presents cues in capitals.";
    // writer-subset-1 and -2 admitted no empty line and no leading, trailing or repeated space.
    const bool spacing = (contract == "writer-subset-3" || contract == writer_subset_contract) && admitted_spacing(e);
    for (const auto gap : writer_subset_gaps(e, contract == writer_subset_contract))
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
QStringList fountain_edit_blockers(const ProjectAssets& review, const QString& contract) {
  const auto c = convert_fountain(review);
  auto result = blockers(c, review.original_fountain->size(), contract);
  if (review.original_fountain->contains('\t')) result << "Tabs are outside the verified writer subset.";
  return result;
}
ProjectAssets editable_fountain_assets(const ProjectAssets& review) {
  const auto interpreted = interpreted_fountain_assets(review);
  const auto reasons = fountain_edit_blockers(interpreted);
  if (!reasons.empty()) fail(reasons.join('\n'));
  const QJsonObject root{{"format", "apollo-fountain-working"}, {"version", 1}, {"contract", writer_subset_contract},
      {"intake", QJsonDocument::fromJson(*interpreted.imported_material).object()}};
  return {QJsonDocument(root).toJson(), interpreted.original_fountain};
}
void validate_fountain_working(const ProjectAssets& assets) {
  const auto review = review_of(assets);
  const auto reasons = fountain_edit_blockers(review.assets, review.contract);
  if (!reasons.empty()) fail(reasons.join('\n'));
}
Screenplay initial_fountain_writing(const ProjectAssets& assets) {
  validate_fountain_working(assets);
  return convert_fountain(review_assets(assets)).screenplay();
}
FountainIntake open_working_fountain(const ProjectAssets& assets) {
  validate_fountain_working(assets);
  const auto review = review_assets(assets);
  auto result = open_fountain_intake(review);
  const auto c = convert_fountain(review);
  result.assets = assets;
  result.working = true;
  result.summary = QString("Editable import copy. %1 screenplay elements; %2 retained records.\n"
      "The original and imported records are read-only. Associations refer to original elements, not tracked text ranges; removed anchors are shown as Detached. "
      "Notes, stash, organization and production metadata stay outside screenplay output.\n%3")
      .arg(c.elements.size()).arg(c.records.size()).arg(imported_pdf_block);
  result.findings.clear();
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
  // The frozen interpretation is the record of what the import retained.
  const auto records = convert_fountain(review_assets(assets)).records;
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
  if (parts.isEmpty()) return {};
  return "Not included in clean Fountain: " + parts.join(", ") + ". They stay in this project's imported material and original.";
}
QString imported_origin(const ProjectAssets& assets) {
  if (!assets.imported_material) return {};
  const auto root = QJsonDocument::fromJson(*assets.imported_material).object();
  return (is_fountain_working(assets) ? root.value("intake").toObject() : root).value("origin").toString();
}
}
