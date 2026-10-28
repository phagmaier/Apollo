#include "engine/layout.h"
#include "engine/wrap.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace apollo {
namespace {
constexpr int page_slots = 56;

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

// Return the last allowed sentence break; each fragment is rewrapped separately.
std::size_t action_split(std::string_view text, int available) {
  if (available < 2) return std::string_view::npos;
  auto pos = text.size();
  while (pos > 0) {
    pos = text.rfind(' ', pos - 1);
    if (pos == std::string_view::npos) break;
    if (pos == 0 || pos + 1 == text.size()) continue;
    if (!sentence_end(text.substr(0, pos))) continue;
    const auto before = wrap(text.substr(0, pos), 61);
    const auto after = wrap(text.substr(pos + 1), 61);
    if (before.size() >= 2 && before.size() <= static_cast<std::size_t>(available) && after.size() >= 2) return pos;
  }
  return std::string_view::npos;
}

std::vector<std::string> speech_lines(const Element& element, std::size_t dialogue_width = 35) {
  if (element.type != ElementType::parenthetical) return wrap(element.text, dialogue_width);
  const auto first_wrap = wrap(element.text, 17);
  if (first_wrap.size() < 2) return first_wrap;
  std::vector<std::string> lines{first_wrap.front()};
  auto rest = std::string_view(element.text).substr(first_wrap.front().size());
  const auto tail = wrap(rest, 16);
  lines.insert(lines.end(), tail.begin(), tail.end());
  return lines;
}

struct SpeechBreak { std::size_t part; std::size_t offset; };

// Pick the latest sentence boundary that leaves two dialogue lines on both
// sides, while reserving a slot for (MORE). The fragments are rewrapped.
bool split_speech(const std::vector<Element>& parts, int available, SpeechBreak& chosen) {
  bool found = false;
  int before_slots = 0, before_dialogue = 0;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const auto& part = parts[i];
    if (part.type == ElementType::dialogue) {
      auto consider = [&](std::size_t offset, bool whole) {
        const auto prefix = wrap(std::string_view(part.text).substr(0, offset), 35);
        int after_dialogue = whole ? 0 : static_cast<int>(wrap(std::string_view(part.text).substr(offset + 1), 35).size());
        for (std::size_t j = i + 1; j < parts.size(); ++j)
          if (parts[j].type == ElementType::dialogue) after_dialogue += static_cast<int>(speech_lines(parts[j]).size());
        const int prior = before_dialogue + static_cast<int>(prefix.size());
        if (prior >= 2 && after_dialogue >= 2 && before_slots + static_cast<int>(prefix.size()) + 1 <= available) {
          chosen = {i, offset}; found = true;
        }
      };
      for (std::size_t pos = part.text.find(' '); pos != std::string::npos; pos = part.text.find(' ', pos + 1)) {
        if (pos && pos + 1 < part.text.size() && sentence_end(std::string_view(part.text).substr(0, pos)))
          consider(pos, false);
      }
      if (i + 1 < parts.size() && sentence_end(part.text)) consider(part.text.size(), true);
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
  if (parts.empty() || available < 1) return false;
  if (i < elements.size() && elements[i].type == ElementType::cue && elements[i].dual_right) {
    int left = 1, right = 1;
    for (const auto& part : parts) left += static_cast<int>(speech_lines(part, 28).size());
    for (++i; i < elements.size() && (elements[i].type == ElementType::dialogue || elements[i].type == ElementType::parenthetical); ++i)
      right += static_cast<int>(speech_lines(elements[i], 28).size());
    return right > 1 && std::max(left, right) <= available;
  }
  int needed = 1;
  for (const auto& part : parts) needed += static_cast<int>(speech_lines(part).size());
  SpeechBreak split{};
  return needed <= available || split_speech(parts, available - 1, split);
}

void transfer_styles(const Element& element, Line& line, std::size_t start) {
  const auto end = start + line.text.size();
  for (const auto& style : element.styles) {
    const auto lo = std::max(start, style.start), hi = std::min(end, style.end);
    if (lo < hi) line.styles.push_back({lo - start, hi - start, style.style});
  }
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

DocumentLayout layout_document(const std::vector<Element>& elements) {
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
      std::string text = e.text;
      if (e.type == ElementType::title) for (auto& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      result.push_back({0, title_slot, e.type, text});
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
      continuation = true;
    }
  };
  auto emit_cue = [&](const Element& element, const std::string& text, std::size_t source) {
    result.push_back({page, ++slot, ElementType::cue, text});
    transfer_styles(element, result.back(), 0);
    if (source != no_source) set_source(result.back(), source, 0);
  };
  for (std::size_t index = 0; index < elements.size(); ++index) {
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
    if (element.type == ElementType::cue) {
      std::vector<Element> parts;
      std::vector<std::size_t> part_sources, part_bases;
      std::size_t end = index + 1;
      while (end < elements.size() && (elements[end].type == ElementType::dialogue || elements[end].type == ElementType::parenthetical)) {
        part_sources.push_back(end); part_bases.push_back(0);
        parts.push_back(elements[end++]);
      }
      if (parts.empty()) throw std::runtime_error("cue without speech is not supported");
      if (element.dual_right) throw std::runtime_error("dual dialogue has no left speech");
      auto speaker = [](const std::string& cue) { return cue.substr(0, cue.find(" (")); };
      auto continuation_cue = [](std::string cue) {
        for (std::string_view suffix : {" (CONT’D)", " (CONT'D)"})
          if (cue.ends_with(suffix)) { cue.erase(cue.size() - suffix.size()); break; }
        return cue + " (CONT’D)";
      };
      const std::string cue_text = interrupted && speaker(element.text) == last_speaker ? continuation_cue(element.text) : element.text;
      if (end < elements.size() && elements[end].type == ElementType::cue && elements[end].dual_right) {
        const auto& right = elements[end];
        std::size_t after = end + 1;
        std::vector<Element> right_parts;
        while (after < elements.size() && (elements[after].type == ElementType::dialogue || elements[after].type == ElementType::parenthetical))
          right_parts.push_back(elements[after++]);
        if (right_parts.empty()) throw std::runtime_error("dual dialogue has no right speech");
        // Speech parts directly follow their cue in the input.
        auto column = [&](std::size_t cue_source, const std::string& cue, const std::vector<Element>& speech) {
          std::vector<Line> out{{0, 0, ElementType::cue, cue}};
          transfer_styles(elements[cue_source], out.back(), 0);
          set_source(out.back(), cue_source, 0);
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
        const auto left_lines = column(index, cue_text, parts);
        const auto right_lines = column(end, right.text, right_parts);
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
        int needed = 1;
        for (const auto& part : parts) needed += static_cast<int>(speech_lines(part).size());
        const int available = page_slots - slot - gap;
        if (needed <= available) {
          slot += gap;
          emit_cue(element, continued ? continuation_cue(element.text) : cue_text, continued ? no_source : index);
          for (std::size_t i = 0; i < parts.size(); ++i)
            emit_styled(parts[i], speech_lines(parts[i]), part_sources[i], 0, part_bases[i]);
          break;
        }
        SpeechBreak split{};
        if (split_speech(parts, available - 1, split)) {
          slot += gap;
          emit_cue(element, continued ? continuation_cue(element.text) : cue_text, continued ? no_source : index);
          for (std::size_t i = 0; i < split.part; ++i)
            emit_styled(parts[i], speech_lines(parts[i]), part_sources[i], 0, part_bases[i]);
          auto& part = parts[split.part];
          emit_styled(part, wrap(std::string_view(part.text).substr(0, split.offset), 35),
                      part_sources[split.part], 0, part_bases[split.part]);
          emit(ElementType::more, {"(MORE)"});
          const bool whole = split.offset == part.text.size();
          if (!whole) { discard_prefix(part, split.offset + 1); part_bases[split.part] += split.offset + 1; }
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
          return static_cast<int>(wrap(next.text, 61).size()) <= available ||
                 action_split(next.text, available) != std::string_view::npos;
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
      while (!remaining.empty()) {
        const int gap = slot && !continuation ? 1 : 0;
        const int available = page_slots - slot - gap;
        lines = wrap(remaining, 61);
        if (static_cast<int>(lines.size()) <= available) {
          slot += gap; emit_styled(element, lines, index, static_cast<std::size_t>(remaining.data() - element.text.data())); break;
        }
        const auto split = action_split(remaining, available);
        if (split != std::string_view::npos) {
          slot += gap; emit_styled(element, wrap(remaining.substr(0, split), 61), index, static_cast<std::size_t>(remaining.data() - element.text.data()));
          remaining.remove_prefix(split + 1);
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
      std::size_t start = index - 1;
      while (start && (elements[start].type == ElementType::dialogue || elements[start].type == ElementType::parenthetical)) --start;
      if (elements[start].type == ElementType::cue && elements[start].dual_right && start) {
        --start;
        while (start && (elements[start].type == ElementType::dialogue || elements[start].type == ElementType::parenthetical)) --start;
      }
      // Keep the preceding heading attached when moving a scene's first element.
      if (start && elements[start - 1].type == ElementType::heading) --start;
      if (!start || elements[start - 1].type == ElementType::page_break)
        throw std::runtime_error("transition cannot fit with preceding element");
      auto retry = elements;
      retry.insert(retry.begin() + static_cast<std::ptrdiff_t>(start), {ElementType::page_break, ""});
      // The inserted break shifts later input indexes by one.
      auto retried = layout_document(retry);
      for (auto& line : retried.lines) if (line.source != no_source && line.source > start) --line.source;
      return retried;
    }
    const int gap = slot == 0 || in_speech ? 0 : 1;
    if (slot + gap + static_cast<int>(lines.size()) > page_slots)
      throw std::runtime_error("speech pagination is not implemented");
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
