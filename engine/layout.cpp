#include "engine/layout.h"
#include "engine/wrap.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace apollo {
namespace {
constexpr int page_slots = 56;
// A cue runs from 4.0 in to 7.6 in. A right dual cue runs from 5.6 in to
// 7.6 in; a left one gets the same width, which keeps it clear of the right
// column (Apollo proposal).
constexpr std::size_t cue_width = 36, dual_cue_width = 20;

int cue_line_count(std::string_view cue, std::size_t width = cue_width) {
  return static_cast<int>(std::max<std::size_t>(1, wrap(cue, width).size()));
}

double text_width(std::string_view text) {
  const auto columns = std::count_if(text.begin(), text.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; });
  return static_cast<double>(columns) * profile::advance;
}

void position_line(Line& line) {
  double x = 108;
  const auto width = text_width(line.text);
  if (line.column) {
    x = line.column == 'L' ? 108 : 331.2;
    if (line.type == ElementType::cue) x += 72;
    if (line.type == ElementType::parenthetical) x += line.continuation ? 36 : 28.8;
  } else {
    switch (line.type) {
      case ElementType::cue: case ElementType::more: x = 288; break;
      case ElementType::parenthetical: x = line.continuation ? 252 : 244.8; break;
      case ElementType::dialogue: case ElementType::lyric: x = 216; break;
      case ElementType::transition: x = 511.2 - width; break;
      case ElementType::centered: x = 324 - width / 2; break;
      case ElementType::title: case ElementType::credit: case ElementType::author:
      case ElementType::source: x = 306 - width / 2; break;
      case ElementType::contact: x = 374.4; break;
      default: break;
    }
  }
  line.geometry = {x, 81 + (line.slot - 1) * profile::line_pitch, width};
}

// B9: punctuation may be followed by closing quotes or brackets.
bool sentence_end(std::string_view before_space) {
  while (!before_space.empty()) {
    if (before_space.ends_with("\xE2\x80\x9D") || before_space.ends_with("\xE2\x80\x99")) {
      before_space.remove_suffix(3);
    } else if (before_space.ends_with('"') || before_space.ends_with('\'') ||
               before_space.ends_with(')') || before_space.ends_with(']') || before_space.ends_with('}')) {
      before_space.remove_suffix(1);
    } else break;
  }
  return !before_space.empty() && (before_space.back() == '.' || before_space.back() == '?' || before_space.back() == '!');
}

// Whether text's first authored line has nothing but spaces.
bool starts_blank(std::string_view text) {
  const auto visible = text.find_first_not_of(' ');
  return visible == std::string_view::npos || text[visible] == '\n';
}

// A page split: the first page shows text before cut; the next continues
// from rest. Both fragments are rewrapped.
struct Split { std::size_t cut, rest; };

// B9 candidates in text order: a sentence end followed by a space or a
// deliberate break. Spaces at an authored line's edges are not shown, so a
// sentence that ends its line continues at the next authored line whatever
// spaces surround the break. A split never puts an authored blank line on
// either side of the page break. rest == text.size() when only spaces follow.
std::vector<Split> sentence_splits(std::string_view text) {
  std::vector<Split> result;
  for (auto cut = text.find_first_of(" \n"); cut != std::string_view::npos; cut = text.find_first_of(" \n", cut + 1)) {
    if (cut == 0 || text[cut - 1] == ' ' || text[cut - 1] == '\n' || !sentence_end(text.substr(0, cut))) continue;
    auto after = text.find_first_not_of(' ', cut);
    if (after == std::string_view::npos) { result.push_back({cut, text.size()}); continue; }
    if (text[after] != '\n') { result.push_back({cut, cut + 1}); continue; }
    if (!starts_blank(text.substr(after + 1))) result.push_back({cut, after + 1});
  }
  return result;
}

// Return the last allowed sentence break.
std::optional<Split> action_split(std::string_view text, int available) {
  if (available < 2) return std::nullopt;
  const auto candidates = sentence_splits(text);
  for (auto split = candidates.rbegin(); split != candidates.rend(); ++split) {
    if (split->rest == text.size()) continue;
    const auto before = wrap(text.substr(0, split->cut), 61);
    const auto after = wrap(text.substr(split->rest), 61);
    if (before.size() >= 2 && before.size() <= static_cast<std::size_t>(available) && after.size() >= 2) return *split;
  }
  return std::nullopt;
}

// An element with no text but spaces keeps one blank line.
std::vector<std::string> speech_lines(const Element& element, std::size_t dialogue_width = 35) {
  if (element.type != ElementType::parenthetical) {
    auto lines = wrap(element.text, dialogue_width);
    return lines.empty() ? std::vector<std::string>{""} : lines;
  }
  const auto first_wrap = wrap(element.text, 17);
  if (first_wrap.empty()) return {""};
  if (first_wrap.size() < 2) return first_wrap;
  std::vector<std::string> lines{first_wrap.front()};
  auto rest = std::string_view(element.text).substr(first_wrap.front().size());
  const auto tail = wrap(rest, 16);
  lines.insert(lines.end(), tail.begin(), tail.end());
  return lines;
}

// split.rest == the part's size when the rest of the speech starts with the
// next part.
struct SpeechBreak { std::size_t part; Split split; };

// Pick the latest sentence boundary that leaves two dialogue lines on both
// sides, while reserving a slot for (MORE). The fragments are rewrapped.
bool split_speech(const std::vector<Element>& parts, int available, SpeechBreak& chosen) {
  bool found = false;
  int before_slots = 0, before_dialogue = 0;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const auto& part = parts[i];
    if (part.type == ElementType::dialogue) {
      const std::string_view text = part.text;
      auto consider = [&](Split split) {
        const bool whole = split.rest == text.size();
        // The next page continues with the next part; it must not start blank.
        if (whole && (i + 1 == parts.size() || starts_blank(parts[i + 1].text))) return;
        const auto prefix = wrap(text.substr(0, split.cut), 35);
        int after_dialogue = whole ? 0 : static_cast<int>(wrap(text.substr(split.rest), 35).size());
        for (std::size_t j = i + 1; j < parts.size(); ++j)
          if (parts[j].type == ElementType::dialogue) after_dialogue += static_cast<int>(speech_lines(parts[j]).size());
        const int prior = before_dialogue + static_cast<int>(prefix.size());
        if (prior >= 2 && after_dialogue >= 2 && before_slots + static_cast<int>(prefix.size()) + 1 <= available) {
          chosen = {i, split}; found = true;
        }
      };
      auto candidates = sentence_splits(text);
      // The part's own end is a boundary too; sentence_splits finds it only
      // when spaces follow it.
      if (sentence_end(text)) candidates.push_back({text.size(), text.size()});
      for (const auto split : candidates) consider(split);
      before_dialogue += static_cast<int>(speech_lines(part).size());
    }
    before_slots += static_cast<int>(speech_lines(part).size());
  }
  return found;
}

bool following_speech_fits(const std::vector<Element>& elements, std::size_t cue_index, int available) {
  std::vector<Element> parts;
  auto i = cue_index + 1;
  for (; i < elements.size() &&
       (elements[i].type == ElementType::dialogue || elements[i].type == ElementType::parenthetical); ++i)
    parts.push_back(elements[i]);
  if (available < 1) return false;
  if (parts.empty()) return cue_line_count(elements[cue_index].text) <= available;
  if (i < elements.size() && elements[i].type == ElementType::cue && elements[i].dual_right) {
    int left = cue_line_count(elements[cue_index].text, dual_cue_width), right = cue_line_count(elements[i].text, dual_cue_width);
    const int right_cue = right;
    for (const auto& part : parts) left += static_cast<int>(speech_lines(part, 28).size());
    for (++i; i < elements.size() && (elements[i].type == ElementType::dialogue || elements[i].type == ElementType::parenthetical); ++i)
      right += static_cast<int>(speech_lines(elements[i], 28).size());
    if (right > right_cue) return std::max(left, right) <= available;
  }
  // A heading resets G1, so the cue after one has no generated suffix.
  const int cue_lines = cue_line_count(elements[cue_index].text);
  int needed = cue_lines;
  for (const auto& part : parts) needed += static_cast<int>(speech_lines(part).size());
  SpeechBreak split{};
  return needed <= available || split_speech(parts, available - cue_lines, split);
}

void transfer_styles(const Element& element, Line& line, std::size_t start) {
  const auto end = start + line.text.size();
  for (const auto& style : element.styles) {
    const auto lo = std::max(start, style.start), hi = std::min(end, style.end);
    if (lo < hi) line.styles.push_back({lo - start, hi - start, style.style});
  }
}

// Laid out as presented: same bytes, so offsets and styles still hold.
std::vector<Element> displayed(const std::vector<Element>& elements) {
  auto shown = elements;
  for (auto& element : shown) element.text = presented_text(element.type, std::move(element.text));
  return shown;
}

// Bytes of a typed continuation suffix at the end of an uppercased cue.
std::size_t continuation_suffix(std::string_view cue) {
  for (std::string_view suffix : {" (CONT’D)", " (CONT'D)"})
    if (cue.ends_with(suffix)) return suffix.size();
  return 0;
}

void discard_prefix(Element& element, std::size_t count) {
  element.text.erase(0, count);
  std::vector<StyleSpan> kept;
  for (auto style : element.styles) {
    if (style.end <= count) continue;
    style.start = style.start > count ? style.start - count : 0;
    style.end -= count;
    kept.push_back(style);
  }
  element.styles = std::move(kept);
}
}

bool presented_in_capitals(ElementType type) {
  return type == ElementType::heading || type == ElementType::cue || type == ElementType::title;
}
std::string presented_text(ElementType type, std::string text) {
  if (presented_in_capitals(type))
    for (auto& c : text) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return text;
}

DocumentLayout layout_document(const std::vector<Element>& input) {
  const auto elements = displayed(input);
  std::vector<Line> result;
  int page = 1, slot = 0;
  std::string last_speaker;
  bool interrupted = false;
  int authors = 0, contacts = 0;
  for (const auto& e : elements) if (e.type == ElementType::contact) ++contacts;
  for (const auto& e : elements) {
    const auto source = static_cast<std::size_t>(&e - elements.data());
    int title_slot = 0;
    switch (e.type) {
      case ElementType::title: title_slot = 4; break;
      case ElementType::credit: title_slot = 10; break;
      case ElementType::author: title_slot = 15 + authors++; break;
      case ElementType::source: title_slot = 16 + std::max(authors, 1); break;
      case ElementType::contact: title_slot = 49 - contacts--; break;
      case ElementType::draft_date: title_slot = 48; break;
      default: break;
    }
    if (title_slot) {
      result.push_back({0, title_slot, e.type, e.text});
      transfer_styles(e, result.back(), 0);
      result.back().source = source; result.back().source_length = e.text.size();
    }
  }
  auto next_page = [&] { ++page; slot = 0; };
  auto emit = [&](ElementType type, const std::vector<std::string>& lines) {
    for (const auto& line : lines) result.push_back({page, ++slot, type, line});
  };
  // Map the line's leading text to elements[source]; stop at a generated suffix.
  auto set_source = [&](Line& line, std::size_t source, std::size_t offset) {
    const auto& text = elements[source].text;
    std::size_t length = 0;
    while (length < line.text.size() && offset + length < text.size() && line.text[length] == text[offset + length]) ++length;
    while (length && length < line.text.size() && (static_cast<unsigned char>(line.text[length]) & 0xc0) == 0x80) --length;
    line.source = source; line.source_offset = offset; line.source_length = length;
  };
  // base counts source bytes already removed from a split speech part.
  auto emit_styled = [&](const Element& element, const std::vector<std::string>& lines, std::size_t source,
                         std::size_t offset = 0, std::size_t base = 0) {
    bool continuation = false;
    for (const auto& text : lines) {
      const auto pos = element.text.find(text, offset);
      if (pos == std::string::npos) throw std::runtime_error("layout text does not map to its source");
      result.push_back({page, ++slot, element.type, text, 0, continuation});
      transfer_styles(element, result.back(), pos);
      set_source(result.back(), source, base + pos);
      offset = pos + text.size();
      auto next = offset;
      while (next < element.text.size() && element.text[next] == ' ') ++next;
      if (next < element.text.size() && element.text[next] == '\n') offset = next + 1;
      continuation = true;
    }
  };
  // A cue's lines, wrapped at width. Text past the source cue, such as a
  // generated (CONT’D), is generated; it replaces the last typed_suffix
  // source bytes.
  auto cue_lines = [&](const Element& element, const std::string& text, std::size_t source, std::size_t typed_suffix,
                       std::size_t width) {
    const auto base = element.text.size() - typed_suffix;
    auto texts = wrap(text, width);
    if (texts.empty()) texts = {""};
    std::vector<Line> lines;
    std::size_t offset = 0, last_mapped = no_source;
    for (const auto& line_text : texts) {
      const auto pos = text.find(line_text, offset);
      offset = pos + line_text.size();
      lines.push_back({0, 0, ElementType::cue, line_text});
      auto& line = lines.back();
      transfer_styles(element, line, pos);
      if (source == no_source || (pos >= base && !line_text.empty())) continue;
      set_source(line, source, pos);
      line.source_length = std::min(line.source_length, base - pos);
      last_mapped = lines.size() - 1;
    }
    if (typed_suffix && last_mapped != no_source) {
      lines[last_mapped].source_length = base - lines[last_mapped].source_offset;
      lines[last_mapped].replaced_length = typed_suffix;
    }
    return lines;
  };
  auto emit_cue = [&](const Element& element, const std::string& text, std::size_t source, std::size_t typed_suffix = 0) {
    for (auto& line : cue_lines(element, text, source, typed_suffix, cue_width)) {
      line.page = page; line.slot = ++slot;
      result.push_back(std::move(line));
    }
  };
  // State before each element, so a transition that does not fit can resume
  // from the element it moves instead of laying out the document again.
  struct Resume { std::size_t lines; int page, slot; std::string last_speaker; bool interrupted; };
  std::vector<Resume> resume(elements.size());
  std::vector<bool> break_before(elements.size());
  std::vector<std::size_t> dual_starts(elements.size(), no_source);
  for (std::size_t index = 0; index < elements.size(); ++index) {
    resume[index] = {result.size(), page, slot, last_speaker, interrupted};
    const auto& element = elements[index];
    if (element.type == ElementType::title || element.type == ElementType::credit || element.type == ElementType::author ||
        element.type == ElementType::source || element.type == ElementType::contact || element.type == ElementType::draft_date) continue;
    if (element.type == ElementType::page_break) { next_page(); continue; }
    const bool in_speech = element.type == ElementType::parenthetical || element.type == ElementType::dialogue;
    const std::size_t width = element.type == ElementType::dialogue || element.type == ElementType::lyric ? 35 :
                              element.type == ElementType::parenthetical ? 17 :
                              element.type == ElementType::centered ? 60 :
                              element.type == ElementType::transition ? 56 : 61;
    auto lines = wrap(element.text, width);
    if (lines.empty()) lines = {""};  // An element without text keeps one blank slot.
    if (element.type == ElementType::cue) {
      std::vector<Element> parts;
      std::vector<std::size_t> part_sources, part_bases;
      std::size_t end = index + 1;
      while (end < elements.size() && (elements[end].type == ElementType::dialogue || elements[end].type == ElementType::parenthetical)) {
        part_sources.push_back(end); part_bases.push_back(0);
        parts.push_back(elements[end++]);
      }
      // A right cue reached here has no left speech, so it is an ordinary cue.
      auto speaker = [](const std::string& cue) { return cue.substr(0, cue.find(" (")); };
      auto continuation_cue = [](std::string cue) {
        cue.erase(cue.size() - continuation_suffix(cue));
        return cue + " (CONT’D)";
      };
      // G1 replaces a typed continuation suffix rather than doubling it.
      const bool g1 = interrupted && speaker(element.text) == last_speaker;
      const std::string cue_text = g1 ? continuation_cue(element.text) : element.text;
      const std::size_t typed_suffix = g1 ? continuation_suffix(element.text) : 0;
      if (parts.empty()) {
        // A cue without speech yet stands alone and moves whole.
        const int needed = cue_line_count(cue_text);
        if (needed > page_slots) throw std::runtime_error("cue without speech exceeds one page");
        int gap = slot ? 1 : 0;
        if (slot + gap + needed > page_slots) { next_page(); gap = 0; }
        slot += gap;
        emit_cue(element, cue_text, index, typed_suffix);
        last_speaker = speaker(element.text); interrupted = false;
        continue;
      }
      std::size_t after = end + 1;
      std::vector<Element> right_parts;
      if (end < elements.size() && elements[end].type == ElementType::cue && elements[end].dual_right)
        while (after < elements.size() && (elements[after].type == ElementType::dialogue || elements[after].type == ElementType::parenthetical))
          right_parts.push_back(elements[after++]);
      // A right cue without speech is laid out after this speech, not as dual.
      if (!right_parts.empty()) {
        dual_starts[end] = index;
        const auto& right = elements[end];
        // Speech parts directly follow their cue in the input.
        auto column = [&](std::size_t cue_source, const std::string& cue, std::size_t suffix, const std::vector<Element>& speech) {
          auto out = cue_lines(elements[cue_source], cue, cue_source, suffix, dual_cue_width);
          for (std::size_t k = 0; k < speech.size(); ++k) {
            const auto& part = speech[k];
            std::size_t offset = 0;
            bool continuation = false;
            for (const auto& text : speech_lines(part, 28)) {
              const auto pos = part.text.find(text, offset);
              if (pos == std::string::npos) throw std::runtime_error("dual dialogue does not map to its source");
              out.push_back({0, 0, part.type, text, 0, continuation});
              transfer_styles(part, out.back(), pos);
              set_source(out.back(), cue_source + 1 + k, pos);
              offset = pos + text.size(); continuation = true;
            }
          }
          return out;
        };
        const auto left_lines = column(index, cue_text, typed_suffix, parts);
        const auto right_lines = column(end, right.text, 0, right_parts);
        const auto height = std::max(left_lines.size(), right_lines.size());
        if (height > page_slots) throw std::runtime_error("dual dialogue exceeds one page");
        int gap = slot ? 1 : 0;
        if (slot + gap + static_cast<int>(height) > page_slots) { next_page(); gap = 0; }
        slot += gap;
        for (std::size_t row = 0; row < height; ++row) {
          ++slot;
          if (row < left_lines.size()) { auto line = left_lines[row]; line.page = page; line.slot = slot; line.column = 'L'; result.push_back(std::move(line)); }
          if (row < right_lines.size()) { auto line = right_lines[row]; line.page = page; line.slot = slot; line.column = 'R'; result.push_back(std::move(line)); }
        }
        last_speaker = speaker(right.text); interrupted = false;
        index = after - 1;
        continue;
      }
      bool continued = false;
      while (!parts.empty()) {
        int gap = slot ? 1 : 0;
        const auto shown_cue = continued ? continuation_cue(element.text) : cue_text;
        const int cue_lines = cue_line_count(shown_cue);
        int needed = cue_lines;
        for (const auto& part : parts) needed += static_cast<int>(speech_lines(part).size());
        const int available = page_slots - slot - gap;
        if (needed <= available) {
          slot += gap;
          emit_cue(element, shown_cue, continued ? no_source : index, continued ? 0 : typed_suffix);
          for (std::size_t i = 0; i < parts.size(); ++i)
            emit_styled(parts[i], speech_lines(parts[i]), part_sources[i], 0, part_bases[i]);
          break;
        }
        SpeechBreak split{};
        if (split_speech(parts, available - cue_lines, split)) {
          slot += gap;
          emit_cue(element, shown_cue, continued ? no_source : index, continued ? 0 : typed_suffix);
          for (std::size_t i = 0; i < split.part; ++i)
            emit_styled(parts[i], speech_lines(parts[i]), part_sources[i], 0, part_bases[i]);
          auto& part = parts[split.part];
          emit_styled(part, wrap(std::string_view(part.text).substr(0, split.split.cut), 35),
                      part_sources[split.part], 0, part_bases[split.part]);
          emit(ElementType::more, {"(MORE)"});
          const bool whole = split.split.rest == part.text.size();
          if (!whole) { discard_prefix(part, split.split.rest); part_bases[split.part] += split.split.rest; }
          const auto consumed = static_cast<std::ptrdiff_t>(split.part + (whole ? 1 : 0));
          parts.erase(parts.begin(), parts.begin() + consumed);
          part_sources.erase(part_sources.begin(), part_sources.begin() + consumed);
          part_bases.erase(part_bases.begin(), part_bases.begin() + consumed);
          next_page(); continued = true;
        } else {
          if (slot == 0) throw std::runtime_error("speech cannot be split according to profile");
          next_page();
        }
      }
      last_speaker = speaker(element.text); interrupted = false;
      index = end - 1;
      continue;
    }

    if (element.type == ElementType::heading) {
      last_speaker.clear(); interrupted = false;
      int start = slot + (slot ? 2 : 0);
      auto following_fits = [&](int available) {
        if (index + 1 == elements.size()) return true;
        const auto& next = elements[index + 1];
        if (next.type == ElementType::action)
          return static_cast<int>(wrap(next.text, 61).size()) <= available || action_split(next.text, available);
        if (next.type == ElementType::cue) return following_speech_fits(elements, index + 1, available);
        if (next.type == ElementType::centered || next.type == ElementType::lyric)
          return static_cast<int>(wrap(next.text, next.type == ElementType::centered ? 60 : 35).size()) <= available;
        return true;
      };
      bool fits = start + static_cast<int>(lines.size()) < page_slots &&
                  following_fits(page_slots - start - static_cast<int>(lines.size()) - 1);
      if (!fits && slot) {
        next_page();
        start = 0;
        fits = static_cast<int>(lines.size()) < page_slots &&
               following_fits(page_slots - static_cast<int>(lines.size()) - 1);
      }
      if (!fits && slot == 0) throw std::runtime_error("heading and following content cannot fit");
      if (start + static_cast<int>(lines.size()) >= page_slots)
        throw std::runtime_error("heading does not fit with following content");
      slot = start;
      emit_styled(element, lines, index);
      continue;
    }

    if (element.type == ElementType::action) {
      if (!last_speaker.empty()) interrupted = true;
      std::string_view remaining = element.text;
      bool continuation = false;
      if (remaining.find_first_not_of(' ') == std::string_view::npos) {
        int gap = slot ? 1 : 0;
        if (slot + gap + 1 > page_slots) { next_page(); gap = 0; }
        slot += gap;
        emit_styled(element, lines, index);
        remaining = {};
      }
      while (!remaining.empty()) {
        const int gap = slot && !continuation ? 1 : 0;
        const int available = page_slots - slot - gap;
        lines = wrap(remaining, 61);
        if (static_cast<int>(lines.size()) <= available) {
          slot += gap; emit_styled(element, lines, index, static_cast<std::size_t>(remaining.data() - element.text.data())); break;
        }
        const auto split = action_split(remaining, available);
        if (split) {
          slot += gap; emit_styled(element, wrap(remaining.substr(0, split->cut), 61), index, static_cast<std::size_t>(remaining.data() - element.text.data()));
          remaining.remove_prefix(split->rest);
          continuation = true;
          next_page();
        } else {
          if (slot == 0) throw std::runtime_error("action cannot be split according to profile");
          next_page();
          continuation = true;
        }
      }
      continue;
    }

    if (element.type == ElementType::lyric || element.type == ElementType::centered) {
      // The Academy ending has two blank slots between FADE OUT. and the
      // centered ending. Other centered text and lyrics use the normal gap.
      int gap = slot ? (element.type == ElementType::centered && index &&
                        elements[index - 1].type == ElementType::transition ? 2 : 1) : 0;
      if (lines.size() > page_slots) throw std::runtime_error("unsplittable element exceeds one page");
      if (slot + gap + static_cast<int>(lines.size()) > page_slots) { next_page(); gap = 0; }
      slot += gap;
      emit_styled(element, lines, index);
      if (!last_speaker.empty()) interrupted = true;
      continue;
    }
    if (element.type == ElementType::transition && slot + (slot ? 1 : 0) + static_cast<int>(lines.size()) > page_slots) {
      if (index == 0) throw std::runtime_error("transition cannot start a page");
      auto speech_part = [&](std::size_t at) {
        return !break_before[at] && (elements[at].type == ElementType::dialogue || elements[at].type == ElementType::parenthetical);
      };
      std::size_t start = index - 1;
      while (start && speech_part(start)) --start;
      // Only an actual right column belongs to the preceding speech. An
      // unpaired right cue is ordinary, including during this retry.
      if (dual_starts[start] != no_source && !break_before[start]) start = dual_starts[start];
      // Keep the preceding heading attached when moving a scene's first element.
      if (start && !break_before[start] && elements[start - 1].type == ElementType::heading) --start;
      if (!start || break_before[start] || elements[start - 1].type == ElementType::page_break)
        throw std::runtime_error("transition cannot fit with preceding element");
      // Resume as if a manual break preceded the moved element.
      break_before[start] = true;
      const auto& state = resume[start];
      result.resize(state.lines);
      page = state.page; slot = state.slot; last_speaker = state.last_speaker; interrupted = state.interrupted;
      next_page();
      index = start - 1;
      continue;
    }
    if (in_speech) {
      // Speech parts reach here only without a cue. They keep the speech's
      // spacing and move whole.
      std::size_t run_end = index;
      int needed = 0;
      for (; run_end < elements.size() && (elements[run_end].type == ElementType::dialogue ||
                                           elements[run_end].type == ElementType::parenthetical); ++run_end)
        needed += static_cast<int>(speech_lines(elements[run_end]).size());
      if (needed > page_slots) throw std::runtime_error("speech without a cue exceeds one page");
      int gap = slot ? 1 : 0;
      if (slot + gap + needed > page_slots) { next_page(); gap = 0; }
      slot += gap;
      for (; index < run_end; ++index) emit_styled(elements[index], speech_lines(elements[index]), index);
      --index;
      continue;
    }
    const int gap = slot == 0 ? 0 : 1;
    if (slot + gap + static_cast<int>(lines.size()) > page_slots)
      throw std::runtime_error("element does not fit on an empty page");
    slot += gap;
    emit_styled(element, lines, index);
  }
  DocumentLayout document{std::move(result), {}};
  for (auto& line : document.lines) position_line(line);
  if (std::any_of(document.lines.begin(), document.lines.end(), [](const Line& line) { return line.page == 0; }))
    document.pages.push_back({0, std::nullopt});
  // Preserve empty pages, including consecutive and trailing manual breaks.
  for (int number = 1; number <= page; ++number) {
    Page placed{number, std::nullopt};
    if (number > 1) {
      const auto text = std::to_string(number) + ".";
      const auto width = text_width(text);
      placed.number = PositionedText{text, {522 - width, 45, width}};
    }
    document.pages.push_back(std::move(placed));
  }
  return document;
}

std::vector<Line> layout(const std::vector<Element>& elements) {
  return layout_document(elements).lines;
}
}
