#include "source/fountain_conversion.h"
#include "source/fountain_intake.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <algorithm>

namespace apollo::source {
namespace {
QJsonObject location(const FountainRange& r) {
  return {{"startByte", r.start}, {"endByte", r.end}, {"firstLine", r.first_line}, {"lastLine", r.last_line}};
}
struct Text {
  QByteArray bytes;
  std::vector<qsizetype> offsets;
  Text mid(qsizetype start, qsizetype length = -1) const {
    if (length < 0) length = bytes.size() - start;
    return {bytes.mid(start, length), {offsets.begin() + start, offsets.begin() + start + length}};
  }
  Text trimmed(bool right = true) const {
    qsizetype a = 0, b = bytes.size();
    while (a < b && (bytes[a] == ' ' || bytes[a] == '\t')) ++a;
    if (right) while (b > a && (bytes[b - 1] == ' ' || bytes[b - 1] == '\t')) --b;
    return mid(a, b - a);
  }
};
struct Line { Text text; qsizetype start = 0, end = 0; bool hidden = false; };
bool escaped(const QByteArray& bytes, qsizetype at) {
  qsizetype count = 0;
  while (at > 0 && bytes[--at] == '\\') ++count;
  return count % 2;
}
bool upper(const QByteArray& bytes) {
  const auto text = QString::fromUtf8(bytes);
  return text == text.toUpper() && std::any_of(text.begin(), text.end(), [](QChar c) { return c.isLetter(); });
}
class Converter {
public:
  explicit Converter(const QByteArray& bytes) : bytes_(bytes), removed_(bytes.size(), false) {
    for (qsizetype i = 0; i < bytes.size(); ++i) {
      if (bytes[i] == '\r') { if (i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i; starts_.push_back(i + 1); }
      else if (bytes[i] == '\n') starts_.push_back(i + 1);
    }
  }
  FountainConversion run();
private:
  int line_at(qsizetype byte) const { return int(std::upper_bound(starts_.begin(), starts_.end(), byte) - starts_.begin()); }
  FountainRange range(qsizetype a, qsizetype b) const { return {a, b, line_at(a), line_at(std::max(a, b - 1))}; }
  FountainRange range(const Text& text) const {
    return text.offsets.empty() ? range(0, 0) : range(text.offsets.front(), text.offsets.back() + 1);
  }
  void issue(QString code, QString message, FountainRange r) { result_.unresolved.push_back({std::move(code), std::move(message), r}); }
  void limit(QString code, QString message, FountainRange r) { result_.limitations.push_back({std::move(code), std::move(message), r}); }
  void remove(qsizetype a, qsizetype b) { std::fill(removed_.begin() + a, removed_.begin() + b, true); }
  FountainRecord& record(QString kind, QString name, QString text, FountainRange r) {
    result_.records.push_back({QString("r%1").arg(result_.records.size() + 1), std::move(kind), std::move(name), std::move(text), {}, {}, {}, 0, r});
    return result_.records.back();
  }
  Element emphasis(const Text& text, ElementType type);
  std::string add(ElementType type, Text text, FountainRange r, bool join = false);
  void delimiters();
  void lines();
  std::size_t title_page();
  void associate();
  const QByteArray& bytes_;
  std::vector<bool> removed_;
  std::vector<qsizetype> starts_{0};
  std::vector<Line> lines_;
  FountainConversion result_;
};

// Pair only unambiguous delimiters on one authored line. Unmatched markers
// remain literal. Complex crossing/nested same-style runs are reported rather
// than having a speculative Markdown interpretation silently remove text.
Element Converter::emphasis(const Text& source, ElementType type) {
  Element result{type, ""};
  struct Mark { qsizetype a, b, width; char marker; bool open, close; int mate = -1; };
  std::vector<Mark> marks;
  const auto& s = source.bytes;
  for (qsizetype i = 0; i < s.size();) {
    if (s[i] == '\\' && i + 1 < s.size() && QByteArray("*_\\").contains(s[i + 1])) { i += 2; continue; }
    if (s[i] != '*' && s[i] != '_') { ++i; continue; }
    const auto a = i;
    while (i < s.size() && s[i] == s[a]) ++i;
    const auto width = i - a;
    if (width > (s[a] == '_' ? 1 : 3)) {
      issue("emphasis-run", "Unsupported emphasis delimiter run; retained literally.", range(source.offsets[a], source.offsets[i - 1] + 1));
      continue;
    }
    const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\n'; };
    marks.push_back({a, i, width, s[a], i < s.size() && !space(s[i]), a > 0 && !space(s[a - 1])});
  }
  std::vector<int> stack;
  for (int i = 0; i < int(marks.size()); ++i) {
    auto& m = marks[i];
    if (m.close && !stack.empty() && marks[stack.back()].marker == m.marker && marks[stack.back()].width == m.width) {
      const auto open = stack.back(); stack.pop_back();
      const auto word_char = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
      const auto& opening = marks[open];
      if (opening.a > 0 && opening.b < s.size() && word_char(s[opening.a - 1]) && word_char(s[opening.b])) {
        issue("intraword-emphasis", "Emphasis inside a word is outside the supported subset; markup is retained literally.",
            range(source.offsets[opening.a], source.offsets[m.b - 1] + 1));
        result.text = source.bytes.toStdString(); return result;
      }
      marks[open].mate = i; m.mate = open;
    } else {
      if (m.close && std::any_of(stack.begin(), stack.end(), [&](int p) { return marks[p].marker == m.marker && marks[p].width == m.width; })) {
        issue("crossing-emphasis", "Crossing emphasis is ambiguous; markup is retained literally.", range(source));
        result.text = source.bytes.toStdString(); return result;
      }
      if (m.close && !stack.empty() && marks[stack.back()].marker == m.marker && marks[stack.back()].width != m.width) {
        issue("mixed-emphasis", "Unequal emphasis delimiter runs need a fuller Markdown interpretation; markup is retained literally.",
            range(source.offsets[marks[stack.back()].a], source.offsets[m.b - 1] + 1));
        result.text = source.bytes.toStdString(); return result;
      }
      if (m.open) stack.push_back(i);
    }
  }
  std::vector<qsizetype> out(s.size() + 1);
  std::size_t next_mark = 0;
  for (qsizetype i = 0; i < s.size();) {
    out[i] = qsizetype(result.text.size());
    while (next_mark < marks.size() && marks[next_mark].a < i) ++next_mark;
    const auto mark = next_mark < marks.size() && marks[next_mark].a == i && marks[next_mark].mate >= 0 ? &marks[next_mark] : nullptr;
    if (mark) {
      for (auto j = i; j < mark->b; ++j) out[j] = qsizetype(result.text.size());
      i = mark->b;
    } else if (s[i] == '\\' && i + 1 < s.size() && QByteArray("*_\\").contains(s[i + 1])) {
      out[i + 1] = qsizetype(result.text.size()); result.text += s[i + 1]; i += 2;
    } else { result.text += s[i]; ++i; }
  }
  out[s.size()] = qsizetype(result.text.size());
  for (int i = 0; i < int(marks.size()); ++i) {
    const auto& m = marks[i];
    if (m.mate <= i) continue;
    const auto a = std::size_t(out[m.b]), b = std::size_t(out[marks[m.mate].a]);
    if (a == b) continue;
    if (m.marker == '_') result.styles.push_back({a, b, Style::underline});
    else {
      if (m.width != 2) result.styles.push_back({a, b, Style::italic});
      if (m.width >= 2) result.styles.push_back({a, b, Style::bold});
    }
  }
  return result;
}

std::string Converter::add(ElementType type, Text text, FountainRange r, bool join) {
  auto element = emphasis(text, type);
  std::vector<FountainRange> sources;
  for (const auto offset : text.offsets) {
    if (!sources.empty() && sources.back().end == offset) sources.back().end = offset + 1;
    else sources.push_back(range(offset, offset + 1));
  }
  if (join && !result_.elements.empty() && result_.elements.back().element.type == type) {
    auto& previous = result_.elements.back();
    const auto offset = previous.element.text.size() + 1;
    previous.element.text += '\n' + element.text;
    for (auto span : element.styles) { span.start += offset; span.end += offset; previous.element.styles.push_back(span); }
    previous.range.end = r.end; previous.range.last_line = r.last_line;
    previous.text_sources.insert(previous.text_sources.end(), sources.begin(), sources.end());
    limit("deliberate-break-layout", "Authored line break retained; editor/PDF line-break contract is pending.", r);
    return previous.element.id;
  }
  element.id = "f" + std::to_string(result_.elements.size() + 1);
  result_.elements.push_back({std::move(element), r, std::move(sources)});
  if (type == ElementType::cue || type == ElementType::heading || type == ElementType::title)
    limit("authored-case", "Authored case retained; current rendering applies uppercase defaults.", r);
  return result_.elements.back().element.id;
}

void Converter::delimiters() {
  int notes = 0, stash = 0;
  for (qsizetype i = 0; i + 1 < bytes_.size();) {
    const auto pair = bytes_.mid(i, 2);
    if (pair != "[[" && pair != "/*" && pair != "]]" && pair != "*/") { ++i; continue; }
    if (escaped(bytes_, i)) {
      issue("escaped-delimiter", "Escaped note/boneyard delimiter is outside the supported interpretation; retained literally.", range(i - 1, i + 2));
      i += 2; continue;
    }
    if (pair == "]]" || pair == "*/") {
      issue("orphan-delimiter", "Closing delimiter has no opening delimiter.", range(i, i + 2)); i += 2; continue;
    }
    const bool note = pair == "[[";
    qsizetype boundary = bytes_.size(), close = -1, nested = -1;
    for (qsizetype j = i + 2; j < bytes_.size(); ++j) {
      // Notes never consume a later paragraph. CRLF counts as one newline.
      if (note && (bytes_[j] == '\r' || bytes_[j] == '\n')) {
        auto next = j + 1;
        if (bytes_[j] == '\r' && next < bytes_.size() && bytes_[next] == '\n') ++next;
        auto end_line = next;
        while (end_line < bytes_.size() && bytes_[end_line] != '\r' && bytes_[end_line] != '\n') ++end_line;
        const auto blank = bytes_.mid(next, end_line - next);
        if (blank.trimmed().isEmpty() && (blank.size() < 2 || blank.contains('\t'))) { boundary = j; break; }
      }
      if (escaped(bytes_, j)) continue;
      if (bytes_.mid(j, 2) == (note ? "]]" : "*/")) { close = j; break; }
      if (bytes_.mid(j, 2) == pair && nested < 0) nested = j;
    }
    const auto end = close < 0 ? boundary : close + 2;
    if (close < 0 || nested >= 0) {
      issue(nested >= 0 ? "nested-delimiter" : "unclosed-delimiter",
          "Delimiter interpretation is unresolved; this source range is excluded from the interpreted screenplay.", range(i, end));
    } else {
      const auto name = QString("Imported %1 %2 (line %3)").arg(note ? "note" : "stash").arg(note ? ++notes : ++stash).arg(line_at(i));
      record(note ? "note" : "stash", name, QString::fromUtf8(bytes_.mid(i + 2, close - i - 2)), range(i, end));
    }
    remove(i, end); i = end;
  }
}

void Converter::lines() {
  Line line;
  for (qsizetype i = bytes_.startsWith("\xef\xbb\xbf") ? 3 : 0; i < bytes_.size(); ++i) {
    if (removed_[i]) { line.hidden = true; continue; }
    if (bytes_[i] == '\r' || bytes_[i] == '\n') {
      line.end = i;
      lines_.push_back(std::move(line));
      if (bytes_[i] == '\r' && i + 1 < bytes_.size() && bytes_[i + 1] == '\n') ++i;
      line = {}; line.start = i + 1;
    } else { line.text.bytes += bytes_[i]; line.text.offsets.push_back(i); }
  }
  line.end = bytes_.size(); lines_.push_back(std::move(line));
  // A line occupied solely by removed material is not a dialogue separator.
  std::erase_if(lines_, [](const Line& l) { return l.hidden && l.text.bytes.trimmed().isEmpty(); });
}

std::size_t Converter::title_page() {
  static const QRegularExpression key("^([A-Za-z][A-Za-z ]*):[ \\t]*(.*)$");
  const auto field = [](const QString& name) -> std::optional<ElementType> {
    if (name == "title") return ElementType::title;
    if (name == "credit") return ElementType::credit;
    if (name == "author" || name == "authors") return ElementType::author;
    if (name == "source") return ElementType::source;
    if (name == "contact") return ElementType::contact;
    if (name == "draft date") return ElementType::draft_date;
    return {};
  };
  std::size_t i = 0;
  while (i < lines_.size()) {
    const auto& first = lines_[i];
    const auto match = key.match(QString::fromUtf8(first.text.bytes));
    if (!match.hasMatch()) break;
    const auto name = match.captured(1).trimmed().toLower();
    const auto type = field(name);
    const auto colon = first.text.bytes.indexOf(':');
    auto value = first.text.mid(colon + 1).trimmed();
    // A bare uppercase screenplay label at the opening is not enough evidence
    // for an unknown title field (CUT TO: is also a standard transition).
    if (!type && value.bytes.isEmpty() && (i + 1 == lines_.size() ||
        !(lines_[i + 1].text.bytes.startsWith("   ") || lines_[i + 1].text.bytes.startsWith('\t')))) {
      if (!(first.text.bytes.endsWith("TO:") && upper(first.text.bytes)))
        issue("title-or-action", "Opening bare unknown title key could be screenplay text; retained as action pending review.", range(first.start, first.end));
      break;
    }
    auto end = first.end;
    ++i;
    const auto begin = i;
    if (!value.bytes.isEmpty() && type) add(*type, value, range(first.start, first.end));
    while (i < lines_.size() && (lines_[i].text.bytes.startsWith("   ") || lines_[i].text.bytes.startsWith('\t'))) {
      const auto& continuation = lines_[i++];
      if (type) add(*type, continuation.text.trimmed(), range(continuation.start, continuation.end));
      end = continuation.end;
    }
    if (type && value.bytes.isEmpty() && i == begin) add(*type, value, range(first.start, first.end));
    if (!type) {
      auto& r = record("titleMetadata", "Title metadata: " + match.captured(1),
          QString::fromUtf8(bytes_.mid(first.start, end - first.start)), range(first.start, end));
      r.key = match.captured(1);
    }
  }
  return i;
}

FountainConversion Converter::run() {
  auto body = bytes_;
  if (body.startsWith("\xef\xbb\xbf")) body.remove(0, 3);
  if (QString::fromUtf8(body).toUtf8() != body) {
    issue("encoding", "Explicit encoding conversion is required; no replacement decoding is used.", range(0, bytes_.size()));
    return result_;
  }
  if (bytes_.size() > 1024 * 1024 || starts_.size() > 100000) {
    issue("conversion-limit", "Revision 1 interpretation is bounded to 1 MiB and 100,000 lines; the complete original remains retained for review.", range(0, bytes_.size()));
    return result_;
  }
  // Source v1 cannot represent Unicode paragraph separators. Flag all control
  // and format characters precisely, retaining their bytes in the original.
  qsizetype byte = bytes_.startsWith("\xef\xbb\xbf") ? 3 : 0;
  for (const char32_t cp : QString::fromUtf8(body).toUcs4()) {
    const auto encoded = QString::fromUcs4(&cp, 1).toUtf8();
    const auto category = QChar::category(cp);
    if ((category == QChar::Other_Control && cp != '\n' && cp != '\r' && cp != '\t') || category == QChar::Other_Format || cp == 0x2028 || cp == 0x2029) {
      issue("control-character", "Control/format character needs explicit review.", range(byte, byte + encoded.size()));
      remove(byte, byte + encoded.size());
    }
    byte += encoded.size();
  }
  delimiters(); lines();
  bool boundary = true, speech = false, action = false;
  std::optional<std::size_t> previous_cue;
  std::vector<std::pair<int, QString>> section_stack;
  QString context;
  static const QRegularExpression heading("^(INT\\.?/EXT|INT\\.?|EXT\\.?|EST\\.?|I/E)[. ]", QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression number("[ \\t]+#([A-Za-z0-9.-]+)#[ \\t]*$");
  for (auto i = title_page(); i < lines_.size(); ++i) {
    const auto& line = lines_[i];
    auto text = line.text.trimmed();
    const auto r = range(line.start, line.end);
    const auto raw = line.text.bytes;
    if (raw.isEmpty() || (raw.trimmed().isEmpty() && !(speech && raw.size() >= 2))) {
      if (boundary && !raw.isEmpty()) record("spacing", "Source spacing", QString::fromUtf8(raw), r);
      if (boundary && i > 0 && lines_[i - 1].text.bytes.isEmpty())
        limit("vertical-spacing", "Additional blank line retained in the original; layout spacing is pending.",
            range(line.start, i + 1 < lines_.size() ? lines_[i + 1].start : line.end));
      boundary = true; speech = false; action = false; continue;
    }
    const bool following_blank = i + 1 == lines_.size() || lines_[i + 1].text.bytes.trimmed().isEmpty();
    const auto trimmed = text.bytes;
    const bool forced_action = trimmed.startsWith('!');
    const auto after_dot = trimmed.startsWith('.') ? QString::fromUtf8(trimmed.mid(1)).toUcs4() : QList<uint>{};
    const bool forced_heading = trimmed.startsWith('.') && !after_dot.empty() && QChar::isLetterOrNumber(after_dot.front());
    const bool natural_heading = boundary && following_blank && heading.match(QString::fromUtf8(trimmed)).hasMatch();
    if (trimmed.startsWith("I./E.")) issue("legacy-heading", "Official sample uses I./E., outside the published prefix list; resolve explicitly.", range(text));
    if (trimmed.startsWith('#')) {
      int depth = 0; while (depth < trimmed.size() && trimmed[depth] == '#') ++depth;
      auto& rec = record("section", "Section", QString::fromUtf8(text.mid(depth).trimmed().bytes), r);
      rec.depth = depth;
      while (!section_stack.empty() && section_stack.back().first >= depth) section_stack.pop_back();
      rec.parent = section_stack.empty() ? QString() : section_stack.back().second;
      section_stack.emplace_back(depth, rec.id); context = rec.id;
      speech = action = false; boundary = true; continue;
    }
    if (trimmed.startsWith('=') && !(trimmed.size() >= 3 && std::all_of(trimmed.begin(), trimmed.end(), [](char c) { return c == '='; }))) {
      auto& rec = record("synopsis", "Synopsis", QString::fromUtf8(text.mid(1).trimmed().bytes), r);
      rec.anchor = context; speech = action = false; boundary = true; continue;
    }
    if (forced_heading || natural_heading) {
      if (forced_heading) text = text.mid(1).trimmed();
      const auto scene = number.match(QString::fromUtf8(text.bytes));
      std::optional<QString> production;
      FountainRange production_range;
      if (scene.hasMatch()) {
        const auto start = QString::fromUtf8(text.bytes).left(scene.capturedStart()).toUtf8().size();
        production_range = range(text.mid(start)); production = scene.captured(1);
        text = text.mid(0, start).trimmed();
      } else if (text.bytes.contains('#')) {
        issue("scene-number", "Unrecognized scene-number spelling; retained in heading text pending review.", range(text.mid(text.bytes.indexOf('#'))));
      }
      const auto id = add(ElementType::heading, text, r); context = QString::fromStdString(id);
      if (production) {
        auto& rec = record("sceneNumber", "Production scene number", *production, production_range); rec.anchor = context;
      }
      previous_cue.reset(); speech = action = false; boundary = false; continue;
    }
    if (trimmed.size() >= 3 && std::all_of(trimmed.begin(), trimmed.end(), [](char c) { return c == '='; })) {
      add(ElementType::page_break, text.mid(0, 0), r); previous_cue.reset(); speech = action = false; boundary = false; continue;
    }
    if (trimmed.startsWith('>')) {
      const bool centered = trimmed.endsWith('<');
      add(centered ? ElementType::centered : ElementType::transition, text.mid(1, text.bytes.size() - (centered ? 2 : 1)).trimmed(), r);
      previous_cue.reset(); speech = action = false; boundary = false; continue;
    }
    const auto leading = line.text.trimmed(false);
    if (boundary && following_blank && leading.bytes.endsWith("TO:") && upper(leading.bytes)) {
      add(ElementType::transition, text, r); previous_cue.reset(); speech = action = false; boundary = false; continue;
    }
    bool dual = leading.bytes.endsWith('^');
    if (trimmed.endsWith('^') && !dual)
      issue("dual-trailing-space", "Dual marker is not the last source character; its interpretation needs review.", range(line.text.mid(raw.lastIndexOf('^'))));
    auto cue = dual ? text.mid(0, text.bytes.size() - 1).trimmed() : text;
    const auto extension = cue.bytes.indexOf('(');
    const auto name = cue.bytes.left(extension < 0 ? cue.bytes.size() : extension).trimmed();
    if (!forced_action && (trimmed.startsWith('@') || (boundary && !following_blank && upper(name)))) {
      if (trimmed.startsWith('@')) cue = cue.mid(1);
      const auto cue_name = QString::fromUtf8(cue.bytes.left(cue.bytes.indexOf('(') < 0 ? cue.bytes.size() : cue.bytes.indexOf('(')));
      if (!std::any_of(cue_name.begin(), cue_name.end(), [](QChar c) { return c.isLetter(); }))
        issue("invalid-cue", "Character name has no letter outside its extension.", r);
      const auto cue_index = result_.elements.size();
      add(ElementType::cue, cue, r);
      if (dual) {
        bool has_left = previous_cue && *previous_cue + 1 < cue_index && !result_.elements[*previous_cue].element.dual_right;
        if (has_left) {
          bool dialogue = false;
          for (auto p = *previous_cue + 1; p < cue_index; ++p)
            if (result_.elements[p].element.type == ElementType::dialogue) dialogue = true;
            else if (result_.elements[p].element.type != ElementType::parenthetical) has_left = false;
          has_left = has_left && dialogue;
        }
        if (has_left && !following_blank) result_.elements.back().element.dual_right = true;
        else issue("unpaired-dual", "Dual cue has no unambiguous adjacent speech pair.", range(line.text.mid(line.text.bytes.lastIndexOf('^'))));
      }
      previous_cue = cue_index; speech = true; action = false; boundary = false; continue;
    }
    if (trimmed.startsWith('~')) {
      add(ElementType::lyric, text.mid(1), r);
      if (speech) {
        issue("lyric-in-speech", "Lyric within speech is retained but its speech relationship is not supported by source v1.", r);
        limit("lyric-in-speech", "Lyric/speech layout contract is pending.", r);
      }
      action = false; boundary = false; continue;
    }
    if (speech && !forced_action) {
      const bool parenthetical = trimmed.startsWith('(') && trimmed.endsWith(')');
      add(parenthetical ? ElementType::parenthetical : ElementType::dialogue,
          raw.trimmed().isEmpty() ? line.text : line.text.trimmed(false), r,
          !parenthetical && !result_.elements.empty() && result_.elements.back().element.type == ElementType::dialogue);
      boundary = false; continue;
    }
    auto action_text = forced_action ? line.text.mid(line.text.bytes.indexOf('!') + 1) : line.text;
    if (action_text.bytes.startsWith(' ') || action_text.bytes.startsWith('\t') || action_text.bytes.endsWith(' ') || action_text.bytes.endsWith('\t'))
      limit("action-whitespace", "Authored action spacing retained; whitespace layout/export fidelity is pending.", r);
    add(ElementType::action, action_text, r, action && !forced_action);
    previous_cue.reset(); action = true; speech = false; boundary = false;
  }
  // A forced cue can be the last element. Never call a dangling dual pair valid.
  for (std::size_t i = 0; i < result_.elements.size(); ++i) {
    auto& e = result_.elements[i];
    bool speech_content = false;
    for (auto j = i + 1; e.element.type == ElementType::cue && j < result_.elements.size(); ++j) {
      const auto type = result_.elements[j].element.type;
      if (type == ElementType::dialogue || type == ElementType::lyric) speech_content = true;
      else if (type != ElementType::parenthetical) break;
    }
    if (e.element.type == ElementType::cue && !speech_content) {
      issue("empty-speech", "Character cue has no supported speech.", e.range); e.element.dual_right = false;
    }
  }
  associate();
  return result_;
}

void Converter::associate() {
  for (auto& r : result_.records) {
    if (!r.anchor.isEmpty() || (r.kind != "note" && r.kind != "stash")) continue;
    // Containing element first, otherwise the preceding body element; a leading
    // note points forward. The byte range stays authoritative after later edits.
    const LocatedFountainElement* anchor = nullptr;
    for (const auto& e : result_.elements) {
      if (is_title_field(e.element.type)) continue;
      if (e.range.start <= r.range.start) anchor = &e;
      else { if (!anchor) anchor = &e; break; }
    }
    if (anchor) r.anchor = QString::fromStdString(anchor->element.id);
  }
}
}

Screenplay FountainConversion::screenplay() const {
  Screenplay result;
  for (const auto& e : elements) result.elements.push_back(e.element);
  return result;
}
QJsonObject FountainConversion::json() const {
  QJsonArray located, retained, pending, gaps;
  for (const auto& e : elements) {
    QJsonArray sources;
    for (const auto& r : e.text_sources) sources.append(location(r));
    located.append(QJsonObject{{"id", QString::fromStdString(e.element.id)}, {"range", location(e.range)}, {"textSources", sources}});
  }
  for (const auto& r : records)
    retained.append(QJsonObject{{"id", r.id}, {"kind", r.kind}, {"name", r.name}, {"text", r.text}, {"key", r.key},
        {"anchor", r.anchor}, {"parent", r.parent}, {"depth", r.depth}, {"range", location(r.range)}});
  for (const auto& [code, message, r] : unresolved) pending.append(QJsonObject{{"code", code}, {"message", message}, {"range", location(r)}});
  for (const auto& [code, message, r] : limitations) gaps.append(QJsonObject{{"code", code}, {"message", message}, {"range", location(r)}});
  return {{"revision", 1}, {"screenplay", QJsonDocument::fromJson(serialize_screenplay(screenplay())).object()},
    {"locations", located}, {"retained", retained}, {"unresolved", pending}, {"limitations", gaps},
    {"editable", false}, {"fountainExport", false}, {"pdfExport", false},
    {"outputReason", "Interpretation review only. Authoring/layout fidelity and independent Fountain round-trip are pending; clean screenplay exports are blocked. Retained notes/stash/metadata will be omitted from clean output with disclosure."}};
}
FountainConversion convert_fountain(const ProjectAssets& assets) {
  validate_fountain_intake(assets);
  return Converter(*assets.original_fountain).run();
}
ProjectAssets interpreted_fountain_assets(const ProjectAssets& assets) {
  validate_fountain_intake(assets);
  auto result = assets;
  auto metadata = QJsonDocument::fromJson(*result.imported_material).object();
  metadata.insert("version", 2); metadata.insert("status", "interpreted-review");
  metadata.insert("conversion", convert_fountain(assets).json());
  result.imported_material = QJsonDocument(metadata).toJson();
  if (result.imported_material->size() > maximum_fountain_metadata_bytes)
    throw SourceError(SourceError::Kind::malformed, "Interpretation exceeds the 64 MiB metadata limit; retain the review-only original.");
  return result;
}
void validate_fountain_conversion(const ProjectAssets& assets) {
  if (!assets.original_fountain || !assets.imported_material)
    throw SourceError(SourceError::Kind::malformed, "Fountain interpretation requires metadata and original bytes");
  const auto stored = QJsonDocument::fromJson(*assets.imported_material).object().value("conversion").toObject();
  if (stored.value("revision").toDouble() > 1)
    throw SourceError(SourceError::Kind::newer_version, "Fountain interpretation needs a newer Apollo");
  if (stored != Converter(*assets.original_fountain).run().json())
    throw SourceError(SourceError::Kind::malformed, "Fountain interpretation does not match its immutable original and conversion revision");
}
}
