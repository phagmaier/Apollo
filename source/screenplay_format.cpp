#include "source/screenplay_format.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>

namespace apollo::source {
namespace {
struct Name { ElementType type; const char* name; };
constexpr std::array title_fields{Name{ElementType::title, "title"}, Name{ElementType::credit, "credit"},
                                  Name{ElementType::author, "author"}, Name{ElementType::source, "source"},
                                  Name{ElementType::contact, "contact"}, Name{ElementType::draft_date, "draftDate"}};
constexpr std::array element_types{Name{ElementType::heading, "sceneHeading"}, Name{ElementType::action, "action"},
                                   Name{ElementType::cue, "character"}, Name{ElementType::parenthetical, "parenthetical"},
                                   Name{ElementType::dialogue, "dialogue"}, Name{ElementType::transition, "transition"},
                                   Name{ElementType::lyric, "lyric"}, Name{ElementType::centered, "centered"},
                                   Name{ElementType::page_break, "pageBreak"}};
constexpr std::array style_names{"italic", "bold", "underline"};

template <std::size_t N> const char* name_of(const std::array<Name, N>& names, ElementType type) {
  for (const auto& entry : names) if (entry.type == type) return entry.name;
  return nullptr;
}
template <std::size_t N> std::optional<ElementType> type_named(const std::array<Name, N>& names, const QString& name) {
  for (const auto& entry : names) if (name == QLatin1String(entry.name)) return entry.type;
  return std::nullopt;
}

// Characters a QTextDocument takes as block or frame boundaries, or uses for
// the line breaks that '\n' stands for: text holding one cannot reopen as the
// same elements.
bool structural(char32_t c) { return c == U'\r' || c == 0x2028 || c == 0x2029 || c == 0xfdd0 || c == 0xfdd1; }

bool valid_id(std::string_view id) {
  return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
  });
}

// The byte offset at which each code point starts, then the text's size.
std::vector<std::size_t> code_point_starts(std::string_view text) {
  std::vector<std::size_t> starts;
  for (std::size_t i = 0; i < text.size(); ++i)
    if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) starts.push_back(i);
  starts.push_back(text.size());
  return starts;
}

void put_string(QByteArray& out, std::string_view text) {
  out += '"';
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (byte < 0x20) out += "\\u00" + QByteArray::number(byte, 16).rightJustified(2, '0');
    else out += c;
  }
  out += '"';
}

void put_element(QByteArray& out, const Element& element, bool title_field) {
  if (!valid_id(element.id)) throw std::invalid_argument("element id is missing or invalid: " + element.id);
  const char* name = title_field ? name_of(title_fields, element.type) : name_of(element_types, element.type);
  if (!name) throw std::invalid_argument("element type cannot be saved: " + std::to_string(static_cast<int>(element.type)));
  const auto text = QString::fromStdString(element.text).toStdU32String();
  if (std::any_of(text.begin(), text.end(), structural)) throw std::invalid_argument("element text has an invalid character in " + element.id);
  out += "{\"id\": ";
  put_string(out, element.id);
  out += title_field ? ", \"field\": " : ", \"type\": ";
  put_string(out, name);
  out += ", \"text\": ";
  put_string(out, element.text);
  if (element.dual_right && element.type == ElementType::cue) out += ", \"dual\": true";
  if (!element.styles.empty()) {
    const auto starts = code_point_starts(element.text);
    auto code_point = [&](std::size_t byte) {
      const auto found = std::lower_bound(starts.begin(), starts.end(), byte);
      if (found == starts.end() || *found != byte) throw std::invalid_argument("style span is not on a character boundary in " + element.id);
      return found - starts.begin();
    };
    out += ", \"styles\": [";
    for (std::size_t i = 0; i < element.styles.size(); ++i) {
      const auto& span = element.styles[i];
      if (span.start >= span.end) throw std::invalid_argument("empty style span in " + element.id);
      out += (i ? ", " : "");
      out += "{\"start\": " + QByteArray::number(static_cast<qint64>(code_point(span.start))) +
             ", \"end\": " + QByteArray::number(static_cast<qint64>(code_point(span.end))) + ", \"style\": \"" +
             style_names[static_cast<std::size_t>(span.style)] + "\"}";
    }
    out += ']';
  }
  out += '}';
}

[[noreturn]] void malformed(const QString& message) { throw SourceError(SourceError::Kind::malformed, message); }

void require_keys(const QJsonObject& object, std::initializer_list<const char*> required, std::initializer_list<const char*> optional,
                  const QString& where) {
  for (const char* key : required) if (!object.contains(QLatin1String(key))) malformed(where + QStringLiteral(" has no \"%1\"").arg(key));
  for (auto key = object.begin(); key != object.end(); ++key) {
    auto known = [&](std::initializer_list<const char*> keys) {
      return std::any_of(keys.begin(), keys.end(), [&](const char* name) { return key.key() == QLatin1String(name); });
    };
    if (!known(required) && !known(optional)) malformed(where + QStringLiteral(" has unknown \"%1\"").arg(key.key()));
  }
}

// A JSON number that is a whole number within the range JSON readers keep exactly.
std::optional<std::uint64_t> whole_number(const QJsonValue& value) {
  if (!value.isDouble()) return std::nullopt;
  const double number = value.toDouble();
  if (number < 0 || number > static_cast<double>(max_revision) || std::floor(number) != number) return std::nullopt;
  return static_cast<std::uint64_t>(number);
}

Element parse_element(const QJsonValue& value, bool title_field, const QString& where) {
  if (!value.isObject()) malformed(where + QStringLiteral(" is not an object"));
  const auto object = value.toObject();
  const char* kind = title_field ? "field" : "type";
  require_keys(object, {"id", kind, "text"}, {"dual", "styles"}, where);
  const auto id = object.value("id").toString().toStdString();
  if (!object.value("id").isString() || !valid_id(id)) malformed(where + QStringLiteral(" has an invalid id"));
  const auto type_name = object.value(QLatin1String(kind)).toString();
  const auto type = title_field ? type_named(title_fields, type_name) : type_named(element_types, type_name);
  if (!type) malformed(where + QStringLiteral(" has unknown %1 \"%2\"").arg(kind, type_name));
  if (!object.value("text").isString()) malformed(where + QStringLiteral(" text is not a string"));
  const auto text = object.value("text").toString();
  const auto code_points = text.toStdU32String();
  if (!text.isValidUtf16() || std::any_of(code_points.begin(), code_points.end(), structural))
    malformed(where + QStringLiteral(" text has an invalid character"));
  Element element{*type, text.toStdString(), false, {}, id};
  if (object.contains("dual")) {
    if (!object.value("dual").isBool() || (object.value("dual").toBool() && *type != ElementType::cue))
      malformed(where + QStringLiteral(" has an invalid dual flag"));
    element.dual_right = object.value("dual").toBool();
  }
  if (object.contains("styles")) {
    if (!object.value("styles").isArray()) malformed(where + QStringLiteral(" styles are not a list"));
    const auto starts = code_point_starts(element.text);
    for (const auto& item : object.value("styles").toArray()) {
      const auto span = item.toObject();
      if (!item.isObject()) malformed(where + QStringLiteral(" has an invalid style span"));
      require_keys(span, {"start", "end", "style"}, {}, where + QStringLiteral(" style span"));
      const auto start = whole_number(span.value("start")), end = whole_number(span.value("end"));
      const auto style = std::find_if(style_names.begin(), style_names.end(),
                                      [&](const char* name) { return span.value("style").toString() == QLatin1String(name); });
      if (!start || !end || *start >= *end || *end >= starts.size() || style == style_names.end())
        malformed(where + QStringLiteral(" has an invalid style span"));
      element.styles.push_back({starts[*start], starts[*end], static_cast<Style>(style - style_names.begin())});
    }
  }
  return element;
}
}

bool is_title_field(ElementType type) { return name_of(title_fields, type) != nullptr; }

std::string new_element_id() {
  const auto value = QRandomGenerator::system()->generate64();
  return QByteArray::number(value, 16).rightJustified(16, '0').toStdString();
}

std::vector<StyleSpan> canonical_styles(std::vector<StyleSpan> styles) {
  std::erase_if(styles, [](const StyleSpan& span) { return span.start >= span.end; });
  std::sort(styles.begin(), styles.end(), [](const StyleSpan& a, const StyleSpan& b) {
    return std::tie(a.style, a.start, a.end) < std::tie(b.style, b.start, b.end);
  });
  std::vector<StyleSpan> merged;
  for (const auto& span : styles) {
    if (!merged.empty() && merged.back().style == span.style && span.start <= merged.back().end)
      merged.back().end = std::max(merged.back().end, span.end);
    else merged.push_back(span);
  }
  std::sort(merged.begin(), merged.end(), [](const StyleSpan& a, const StyleSpan& b) {
    return std::tie(a.start, a.end, a.style) < std::tie(b.start, b.end, b.style);
  });
  return merged;
}

Screenplay canonical(Screenplay screenplay) {
  std::stable_partition(screenplay.elements.begin(), screenplay.elements.end(),
                        [](const Element& element) { return is_title_field(element.type); });
  for (auto& element : screenplay.elements) {
    element.styles = canonical_styles(std::move(element.styles));
    element.dual_right = element.dual_right && element.type == ElementType::cue;
  }
  return screenplay;
}

QByteArray serialize_screenplay(const Screenplay& input) {
  if (input.revision > max_revision)
    throw std::invalid_argument("screenplay revision exceeds the format version 1 limit (9007199254740991)");
  const auto screenplay = canonical(input);
  std::set<std::string_view> ids;
  for (const auto& element : screenplay.elements)
    if (!ids.insert(element.id).second) throw std::invalid_argument("element id is repeated: " + element.id);
  QByteArray out;
  out.reserve(static_cast<qsizetype>(screenplay.elements.size() * 96));
  out += "{\n  \"format\": \"apollo-screenplay\",\n  \"version\": " + QByteArray::number(screenplay_version) +
         ",\n  \"revision\": " + QByteArray::number(static_cast<qulonglong>(screenplay.revision)) + ",\n  \"titlePage\": [";
  const auto body = std::find_if(screenplay.elements.begin(), screenplay.elements.end(),
                                 [](const Element& element) { return !is_title_field(element.type); });
  auto put_list = [&](auto from, auto to, bool title_field) {
    for (auto element = from; element != to; ++element) {
      out += element == from ? "\n    " : ",\n    ";
      put_element(out, *element, title_field);
    }
    out += from == to ? "]" : "\n  ]";
  };
  put_list(screenplay.elements.begin(), body, true);
  out += ",\n  \"elements\": [";
  put_list(body, screenplay.elements.end(), false);
  out += "\n}\n";
  return out;
}

Screenplay parse_screenplay(const QByteArray& bytes) {
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError) malformed(QStringLiteral("invalid JSON at byte %1: %2").arg(error.offset).arg(error.errorString()));
  if (!json.isObject()) malformed(QStringLiteral("not an Apollo screenplay"));
  const auto root = json.object();
  if (root.value("format").toString() != QLatin1String("apollo-screenplay")) malformed(QStringLiteral("not an Apollo screenplay"));
  const auto version = whole_number(root.value("version"));
  if (version && *version > static_cast<std::uint64_t>(screenplay_version))
    throw SourceError(SourceError::Kind::newer_version,
                      QStringLiteral("screenplay format version %1 needs a newer Apollo").arg(*version));
  if (version != static_cast<std::uint64_t>(screenplay_version)) malformed(QStringLiteral("unsupported screenplay format version"));
  require_keys(root, {"format", "version", "revision", "titlePage", "elements"}, {}, QStringLiteral("screenplay"));
  const auto revision = whole_number(root.value("revision"));
  if (!revision) malformed(QStringLiteral("invalid revision"));
  if (!root.value("titlePage").isArray() || !root.value("elements").isArray()) malformed(QStringLiteral("element lists are missing"));
  Screenplay screenplay{*revision, {}};
  std::set<std::string> ids;
  auto read_list = [&](const QJsonArray& list, bool title_field) {
    for (qsizetype i = 0; i < list.size(); ++i) {
      const auto where = QStringLiteral("%1 %2").arg(title_field ? "title field" : "element").arg(i + 1);
      auto element = parse_element(list.at(i), title_field, where);
      if (!ids.insert(element.id).second) malformed(where + QStringLiteral(" repeats id \"%1\"").arg(QString::fromStdString(element.id)));
      screenplay.elements.push_back(std::move(element));
    }
  };
  read_list(root.value("titlePage").toArray(), true);
  read_list(root.value("elements").toArray(), false);
  return canonical(std::move(screenplay));
}
}
