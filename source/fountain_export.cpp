#include "source/fountain_export.h"
#include "source/fountain_conversion.h"
#include "source/fountain_working.h"

#include <QRegularExpression>
#include <algorithm>
#include <map>
#include <optional>

namespace apollo::source {
namespace {
QString label(ElementType type) {
  switch (type) {
    case ElementType::heading: return "Scene heading";
    case ElementType::action: return "Action";
    case ElementType::cue: return "Character cue";
    case ElementType::parenthetical: return "Parenthetical";
    case ElementType::dialogue: return "Dialogue";
    case ElementType::page_break: return "Page break";
    case ElementType::more: return "(MORE) marker";
    case ElementType::lyric: return "Lyric";
    case ElementType::transition: return "Transition";
    case ElementType::centered: return "Centered text";
    case ElementType::title: return "Title";
    case ElementType::credit: return "Credit";
    case ElementType::author: return "Author";
    case ElementType::source: return "Source";
    case ElementType::contact: return "Contact";
    case ElementType::draft_date: return "Draft date";
  }
  return "Element";
}
const char* title_key(ElementType type) {
  switch (type) {
    case ElementType::title: return "Title";
    case ElementType::credit: return "Credit";
    case ElementType::author: return "Author";
    case ElementType::source: return "Source";
    case ElementType::contact: return "Contact";
    default: return "Draft date";
  }
}
// "Element 12, Character cue “mara”": numbered in saved order, title page first.
QString describe(const Element& e, std::size_t index) {
  auto first = QString::fromUtf8(e.text).section('\n', 0, 0);
  if (first.size() > 40) first = first.left(40) + QStringLiteral("…");
  const auto quoted = e.type == ElementType::page_break ? QString() : first.isEmpty() ? QStringLiteral(" (empty)") : " “" + first + "”";
  return QString("Element %1, %2%3").arg(index + 1).arg(label(e.type), quoted);
}
bool word(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
bool has_lowercase(const QByteArray& text) {
  return std::any_of(text.begin(), text.end(), [](char c) { return c >= 'a' && c <= 'z'; }) || text.contains("\xc3\xa9");
}
// The frozen interpreter's transition test: capitals with a letter, ending in TO:.
bool reads_as_transition(const QByteArray& line) {
  const auto text = QString::fromUtf8(line);
  return line.endsWith("TO:") && text == text.toUpper() && std::any_of(text.begin(), text.end(), [](QChar c) { return c.isLetter(); });
}
const QRegularExpression& heading_prefix() {
  static const QRegularExpression pattern("^(INT\\.?/EXT|INT\\.?|EXT\\.?|EST\\.?|I/E)[. ]", QRegularExpression::CaseInsensitiveOption);
  return pattern;
}

// One authored line [from, to) of an element with its emphasis as Fountain
// markup: underline _, italic *, bold **, bold italic ***, literal \* \_ and
// a backslash doubled only where it would otherwise escape the next character.
struct Spelled { QByteArray text; QString problem; };
Spelled spell(const std::string& source, std::size_t from, std::size_t to, const std::vector<StyleSpan>& styles) {
  struct Interval { std::size_t a, b; int kind; }; // 0 underline, 1 italic, 2 bold, 3 bold italic
  std::vector<Interval> intervals, italic, bold;
  for (const auto& span : styles) {
    if (span.end <= from || span.start >= to) continue;
    const Interval interval{span.start - from, span.end - from, span.style == Style::underline ? 0 : span.style == Style::italic ? 1 : 2};
    (interval.kind == 0 ? intervals : interval.kind == 1 ? italic : bold).push_back(interval);
  }
  for (const auto& i : italic) {
    const auto same = std::find_if(bold.begin(), bold.end(), [&](const Interval& b) { return b.a == i.a && b.b == i.b; });
    if (same != bold.end()) { intervals.push_back({i.a, i.b, 3}); bold.erase(same); }
    else intervals.push_back(i);
  }
  intervals.insert(intervals.end(), bold.begin(), bold.end());
  const auto text = source.substr(from, to - from);
  for (const auto& i : intervals) {
    if (text[i.a] == ' ' || text[i.b - 1] == ' ') return {{}, "has emphasis that starts or ends with a space"};
    if (i.a > 0 && word(text[i.a - 1]) && word(text[i.a])) return {{}, "has emphasis that begins inside a word"};
  }
  // Outer first; underline outside an equal bold/italic run.
  std::sort(intervals.begin(), intervals.end(), [](const Interval& x, const Interval& y) {
    return x.a != y.a ? x.a < y.a : x.b != y.b ? x.b > y.b : x.kind < y.kind;
  });
  std::map<std::size_t, int> star_ends;
  for (const auto& i : intervals)
    if (i.kind && (++star_ends[i.a] > 1 || ++star_ends[i.b] > 1))
      return {{}, "has bold or italic runs that touch or share a boundary, which Fountain cannot spell unambiguously"};
  std::vector<Interval> open;
  for (const auto& i : intervals) {
    while (!open.empty() && open.back().b <= i.a) open.pop_back();
    if (!open.empty() && i.b > open.back().b) return {{}, "has overlapping emphasis styles where neither contains the other"};
    open.push_back(i);
  }
  static const char* delimiters[] = {"_", "*", "**", "***"};
  std::vector<QByteArray> pieces; // A delimiter or one escaped literal character.
  std::vector<bool> literal_backslash;
  open.clear();
  std::size_t next = 0;
  for (std::size_t p = 0; p <= text.size(); ++p) {
    while (!open.empty() && open.back().b == p) { pieces.push_back(delimiters[open.back().kind]); literal_backslash.push_back(false); open.pop_back(); }
    while (next < intervals.size() && intervals[next].a == p) {
      pieces.push_back(delimiters[intervals[next].kind]); literal_backslash.push_back(false); open.push_back(intervals[next++]);
    }
    if (p == text.size()) break;
    const char c = text[p];
    pieces.push_back(c == '*' ? QByteArray("\\*") : c == '_' ? QByteArray("\\_") : QByteArray(1, c));
    literal_backslash.push_back(c == '\\');
  }
  Spelled result;
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    const bool escapes_next = literal_backslash[i] && i + 1 < pieces.size() && QByteArray("*_\\").contains(pieces[i + 1].front());
    result.text += escapes_next ? QByteArray("\\\\") : pieces[i];
  }
  for (const char* marker : {"[[", "]]", "/*", "*/"})
    if (result.text.contains(marker))
      return {{}, QString("would contain “%1”, which Fountain reads as a note or omitted-passage marker").arg(marker)};
  return result;
}

// Characters that make an unforced line inside a paragraph mean something else.
QString inner_line_problem(const QByteArray& line, bool dialogue) {
  if (line.isEmpty()) return {};
  if (QByteArray("#=>@~!").contains(line.front()))
    return QString("has a line beginning with “%1”, which Fountain reads as markup").arg(QChar(line.front()));
  // Any letter after the dot forces a heading; é is the subset's only non-ASCII letter.
  if (line.startsWith('.') && line.size() > 1 && (word(line[1]) || line.mid(1).startsWith("\xc3\xa9")))
    return "has a line beginning with “.” and a letter or digit, which Fountain reads as a scene heading";
  if (line.startsWith("I./E.")) return "has a line beginning with “I./E.”, which needs review";
  if (dialogue && line.startsWith('(') && line.endsWith(')')) return "has a dialogue line in parentheses, which Fountain reads as a parenthetical";
  return {};
}

class Writer {
public:
  Writer(const std::vector<Element>& snapshot, bool include_title);
  QStringList blockers;
  QByteArray bytes;
  std::vector<Element> written;
  std::vector<std::size_t> numbers;               // Snapshot number of each written element.
  std::vector<std::pair<qsizetype, qsizetype>> ranges; // Output bytes of each written element.
private:
  void block(std::size_t i, const QString& reason) { blockers << describe(written[i], numbers[i]) + ": " + reason + "."; }
  std::vector<QByteArray> lines(std::size_t i, bool& ok);
};

std::vector<QByteArray> Writer::lines(std::size_t i, bool& ok) {
  const auto& e = written[i];
  for (const auto& span : e.styles)
    if (e.text.substr(span.start, span.end - span.start).find('\n') != std::string::npos) {
      block(i, "has emphasis that includes a line break"); ok = false; return {};
    }
  std::vector<QByteArray> result;
  std::size_t from = 0;
  while (true) {
    const auto to = std::min(e.text.find('\n', from), e.text.size());
    auto line = spell(e.text, from, to, e.styles);
    if (!line.problem.isEmpty()) { block(i, line.problem); ok = false; return {}; }
    result.push_back(line.text);
    if (to == e.text.size()) break;
    from = to + 1;
  }
  return result;
}

Writer::Writer(const std::vector<Element>& snapshot, bool include_title) {
  const auto ordered = canonical(Screenplay{0, snapshot}).elements;
  for (std::size_t i = 0; i < ordered.size(); ++i)
    if (include_title || !is_title_field(ordered[i].type)) { written.push_back(ordered[i]); numbers.push_back(i); }
  std::vector<bool> usable(written.size(), true);
  for (std::size_t i = 0; i < written.size(); ++i) {
    const auto& e = written[i];
    if (e.type == ElementType::more) { block(i, "is a generated layout marker, not writing"); usable[i] = false; continue; }
    if (e.type == ElementType::page_break) {
      if (!e.text.empty()) { block(i, "contains text"); usable[i] = false; }
      continue;
    }
    for (const auto gap : writer_subset_gaps(e)) {
      usable[i] = false;
      switch (gap) {
        case WriterSubsetGap::character:
          for (const char32_t cp : QString::fromUtf8(e.text).toUcs4())
            if (!((cp >= 32 && cp <= 126) || cp == '\n' || cp == 0xe9)) {
              block(i, QString("contains “%1” (U+%2); this export is verified for printable ASCII and é")
                  .arg(QString::fromUcs4(&cp, 1), QString::number(cp, 16).toUpper().rightJustified(4, '0')));
              break;
            }
          break;
        case WriterSubsetGap::authored_case: block(i, "is not written in capital ASCII letters, which this export requires for scene headings, character cues and the title"); break;
        case WriterSubsetGap::multiline: block(i, "contains a line break; only action and dialogue can"); break;
        case WriterSubsetGap::long_title: block(i, "is longer than 48 characters"); break;
        case WriterSubsetGap::spacing:
          block(i, e.text.empty() ? QString("is empty; type its text or delete it")
                                  : QString("has an empty line or leading, trailing or repeated spaces"));
          break;
      }
    }
  }
  // Speech structure: a cue owns the parentheticals/dialogue that follow it.
  std::optional<std::size_t> previous_speech_cue;
  for (std::size_t i = 0; i < written.size();) {
    const auto type = written[i].type;
    if (type == ElementType::parenthetical || type == ElementType::dialogue) {
      block(i, "is not part of a speech; put a character cue before it"); usable[i] = false; ++i; continue;
    }
    if (type != ElementType::cue) { previous_speech_cue.reset(); ++i; continue; }
    auto end = i + 1;
    bool dialogue = false;
    for (; end < written.size() && (written[end].type == ElementType::parenthetical || written[end].type == ElementType::dialogue); ++end) {
      if (written[end].type == ElementType::dialogue && written[end - 1].type == ElementType::dialogue) {
        block(end, "follows another dialogue element and would merge with it; join them or put a parenthetical between"); usable[end] = false;
      }
      dialogue = dialogue || written[end].type == ElementType::dialogue;
    }
    if (!dialogue) { block(i, "has no dialogue"); usable[i] = false; }
    if (written[i].dual_right && (!previous_speech_cue || written[*previous_speech_cue].dual_right)) {
      block(i, "is marked as dual dialogue but does not directly follow a single speech to pair with"); usable[i] = false;
    }
    previous_speech_cue = i;
    i = end;
  }
  // Spelling. Title-page runs of one field use indented continuation lines.
  std::vector<std::vector<QByteArray>> paragraphs;
  std::vector<std::vector<std::size_t>> owners; // Element of each paragraph line.
  std::size_t i = 0;
  if (i < written.size() && is_title_field(written[i].type)) {
    paragraphs.emplace_back(); owners.emplace_back();
    while (i < written.size() && is_title_field(written[i].type)) {
      auto end = i;
      while (end < written.size() && written[end].type == written[i].type) ++end;
      const bool several = end - i > 1;
      const auto key = QByteArray(title_key(written[i].type)) + ':';
      if (several) { paragraphs.back().push_back(key); owners.back().push_back(i); }
      for (; i < end; ++i) {
        bool ok = usable[i];
        const auto value = ok ? lines(i, ok) : std::vector<QByteArray>{};
        if (!ok) continue;
        paragraphs.back().push_back(several ? "   " + value.front() : key + ' ' + value.front());
        owners.back().push_back(i);
      }
    }
  }
  const bool title_page = !paragraphs.empty();
  for (; i < written.size(); ++i) {
    const auto& e = written[i];
    bool ok = usable[i];
    auto text = ok ? lines(i, ok) : std::vector<QByteArray>{};
    if (!ok) continue;
    const bool in_speech = e.type == ElementType::parenthetical || e.type == ElementType::dialogue;
    // Without a title page, an opening "Key:" line would start one.
    static const QRegularExpression key("^[A-Za-z][A-Za-z ]*:");
    const bool opening = paragraphs.empty() && !title_page;
    // A speech continues its cue's paragraph. When that cue was blocked the
    // bytes are discarded, but later lines are still checked and explained.
    if (!in_speech || paragraphs.empty()) { paragraphs.emplace_back(); owners.emplace_back(); }
    auto add = [&](const QByteArray& line) { paragraphs.back().push_back(line); owners.back().push_back(i); };
    switch (e.type) {
      case ElementType::heading: {
        const auto& m = text.front();
        if (m.contains('#')) { block(i, "contains “#”, which Fountain reads as a scene number"); break; }
        if (heading_prefix().match(QString::fromUtf8(m)).hasMatch() && !(opening && key.match(QString::fromUtf8(m)).hasMatch())) add(m);
        else if (word(m.front())) add('.' + m);
        else block(i, "must begin with INT., EXT., EST., INT./EXT., I/E or a letter or digit");
        break;
      }
      case ElementType::action: {
        const auto& m = text.front();
        // Unforced only with lowercase before any "(": capitals there read as a cue.
        const bool plain = word(m.front()) && has_lowercase(m.left(m.indexOf('(') < 0 ? m.size() : m.indexOf('('))) && !heading_prefix().match(QString::fromUtf8(m)).hasMatch() &&
                           !(opening && key.match(QString::fromUtf8(m)).hasMatch());
        if (text.size() == 1 && reads_as_transition('!' + m)) { block(i, "is in capitals and ends in TO:, which Fountain reads as a transition"); break; }
        QString problem;
        for (std::size_t line = 1; line < text.size() && problem.isEmpty(); ++line) problem = inner_line_problem(text[line], false);
        if (!problem.isEmpty()) { block(i, problem); break; }
        add(plain ? m : '!' + m);
        for (std::size_t line = 1; line < text.size(); ++line) add(text[line]);
        break;
      }
      case ElementType::cue: {
        const auto& m = text.front();
        const auto name = e.text.substr(0, e.text.find('('));
        if (!std::any_of(name.begin(), name.end(), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); })) {
          block(i, "needs a letter in the character name before any extension"); break;
        }
        if (m.endsWith('^')) { block(i, "ends with “^”, which Fountain reads as a dual-dialogue marker"); break; }
        const bool forced = !word(m.front()) || (opening && key.match(QString::fromUtf8(m)).hasMatch());
        add((forced ? '@' + m : m) + (e.dual_right ? " ^" : ""));
        break;
      }
      case ElementType::parenthetical:
        if (!text.front().startsWith('(') || !text.front().endsWith(')')) { block(i, "must begin with “(” and end with “)” outside any emphasis"); break; }
        add(text.front());
        break;
      case ElementType::dialogue: {
        QString problem;
        for (std::size_t line = 0; line < text.size() && problem.isEmpty(); ++line) problem = inner_line_problem(text[line], true);
        if (!problem.isEmpty()) { block(i, problem); break; }
        for (const auto& line : text) add(line);
        break;
      }
      case ElementType::lyric:
        if (reads_as_transition('~' + text.front())) { block(i, "is in capitals and ends in TO:, which Fountain reads as a transition"); break; }
        add('~' + text.front());
        break;
      case ElementType::transition:
        if (text.front().endsWith('<')) { block(i, "ends with “<”, which Fountain reads as centered text"); break; }
        add('>' + text.front());
        break;
      case ElementType::centered: add("> " + text.front() + " <"); break;
      case ElementType::page_break: add("==="); break;
      default: break;
    }
    if (paragraphs.back().empty()) { paragraphs.pop_back(); owners.pop_back(); }
  }
  ranges.assign(written.size(), {-1, -1});
  for (std::size_t p = 0; p < paragraphs.size(); ++p) {
    if (p) bytes += '\n';
    for (std::size_t l = 0; l < paragraphs[p].size(); ++l) {
      auto& range = ranges[owners[p][l]];
      if (range.first < 0) range.first = bytes.size();
      bytes += paragraphs[p][l] + '\n';
      range.second = bytes.size();
    }
  }
  if (bytes.size() > maximum_fountain_export_bytes)
    blockers << "The Fountain text would exceed 1 MiB, the bound of the reader that verifies each export.";
}
}

QStringList fountain_export_blockers(const std::vector<Element>& snapshot, bool include_title) {
  return Writer(snapshot, include_title).blockers;
}

FountainExport export_fountain(const std::vector<Element>& snapshot, bool include_title) {
  Writer writer(snapshot, include_title);
  if (!writer.blockers.isEmpty()) return {{}, writer.blockers};
  // Never trust the spelling alone: the frozen interpreter must read back the
  // same types, text, emphasis, dual flags and title fields, with nothing left
  // unresolved or diverted into notes, stash or organization.
  const auto read = interpret_fountain(writer.bytes);
  auto at = [&](qsizetype byte) {
    for (std::size_t i = 0; i < writer.written.size(); ++i)
      if (writer.ranges[i].first <= byte && byte < writer.ranges[i].second) return describe(writer.written[i], writer.numbers[i]);
    return QStringLiteral("The export");
  };
  QStringList problems;
  for (const auto& issue : read.unresolved) problems << at(issue.range.start) + ": Fountain verification: " + issue.message;
  for (const auto& gap : read.limitations)
    if (gap.code != "authored-case" && gap.code != "deliberate-break-layout") problems << at(gap.range.start) + ": Fountain verification: " + gap.message;
  for (const auto& record : read.records) problems << at(record.range.start) + ": Fountain verification: would be read as " + record.kind + ".";
  auto plain = [](std::vector<Element> elements) {
    for (auto& e : elements) e.id.clear();
    return canonical(Screenplay{0, std::move(elements)}).elements;
  };
  const auto expected = plain(writer.written), actual = plain(read.screenplay().elements);
  if (problems.isEmpty() && expected != actual) {
    std::size_t k = 0;
    while (k < expected.size() && k < actual.size() && expected[k] == actual[k]) ++k;
    problems << (k < expected.size() ? describe(writer.written[k], writer.numbers[k]) : QStringLiteral("The export")) +
                ": Fountain verification: would not read back unchanged; adjust its wording or punctuation.";
  }
  if (!problems.isEmpty()) return {{}, problems};
  return {writer.bytes, {}};
}
}
