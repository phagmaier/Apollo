#include "engine/wrap.h"
#include "engine/layout.h"
#include "fountain/import.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
  using apollo::wrap;
  int failures = 0;
  auto check = [&](const char* name, const std::vector<std::string>& actual,
                   const std::vector<std::string>& expected) {
    if (actual != expected) {
      std::cerr << name << " failed\n";
      ++failures;
    }
  };
  check("fixture action", wrap("Mara crosses the room, unlatches the tall window, and leans out into the cold.", 61),
        {"Mara crosses the room, unlatches the tall window, and leans", "out into the cold."});
  check("fixture dialogue", wrap("I left the lights on for you. I always leave the lights on.", 35),
        {"I left the lights on for you. I", "always leave the lights on."});
  check("long word", wrap("abcdefghijk next", 5), {"abcde", "fghij", "k", "next"});
  check("unicode columns", wrap("café déjà vu", 5), {"café", "déjà", "vu"});
  check("exact width", wrap("abcde f", 5), {"abcde", "f"});
  check("empty", wrap("", 5), {});
  check("trailing spaces", wrap("cold.  ", 35), {"cold."});
  check("only spaces", wrap("   ", 5), {});
  check("authored breaks", wrap("First line.\nSecond line.", 35), {"First line.", "Second line."});
  check("blank authored lines", wrap("one\n  \n\nlast\n", 35), {"one", "", "", "last", ""});
  check("hard and soft wraps", wrap("café doors open\nnext", 10), {"café doors", "open", "next"});
  try { (void)wrap("text", 0); std::cerr << "zero width accepted\n"; ++failures; }
  catch (const std::invalid_argument&) {}
  using apollo::ElementType;
  const auto lines = apollo::layout({
      {ElementType::heading, "INT. LIGHTHOUSE KITCHEN - NIGHT"},
      {ElementType::action, "Mara crosses the room, unlatches the tall window, and leans out into the cold."},
      {ElementType::cue, "MARA"},
      {ElementType::dialogue, "I left the lights on for you. I always leave the lights on."},
      {ElementType::page_break, ""},
      {ElementType::heading, "EXT. LIGHTHOUSE - NIGHT"},
      {ElementType::action, "The lamp turns."},
  });
  const std::vector<apollo::Line> expected = {
      {1, 1, ElementType::heading, "INT. LIGHTHOUSE KITCHEN - NIGHT"},
      {1, 3, ElementType::action, "Mara crosses the room, unlatches the tall window, and leans"},
      {1, 4, ElementType::action, "out into the cold."},
      {1, 6, ElementType::cue, "MARA"},
      {1, 7, ElementType::dialogue, "I left the lights on for you. I"},
      {1, 8, ElementType::dialogue, "always leave the lights on."},
      {2, 1, ElementType::heading, "EXT. LIGHTHOUSE - NIGHT"},
      {2, 3, ElementType::action, "The lamp turns."},
  };
  if (lines.size() != expected.size()) { std::cerr << "layout line count failed\n"; ++failures; }
  else for (std::size_t i = 0; i < lines.size(); ++i) {
    const auto& a = lines[i]; const auto& e = expected[i];
    if (a.page != e.page || a.slot != e.slot || a.type != e.type || a.text != e.text) {
      std::cerr << "layout line " << i << " failed\n"; ++failures;
    }
  }
  for (const std::string name : {"01-element-geometry", "02-dialogue-split", "03-heading-keep-with-next", "04-dialogue-push", "05-parenthetical-at-break", "06-action-split", "07-title-page-and-manual-break", "08-dual-dialogue-and-contd"}) {
    std::ifstream input(std::string(APOLLO_FIXTURE_DIR) + "/" + name + ".fountain");
    std::ifstream reference(std::string(APOLLO_FIXTURE_DIR) + "/" + name + ".expected");
    if (!input || !reference) { std::cerr << "missing fixture " << name << '\n'; ++failures; continue; }
    std::ostringstream source;
    source << input.rdbuf();
    std::ostringstream actual;
    try {
      int page = -1;
      for (const auto& line : apollo::layout(apollo::import_fountain(source.str()))) {
        if (line.page != page) {
          page = line.page;
          if (page == 0) actual << "page title number=none\n";
          else actual << "page " << page << " number=" << (page == 1 ? "none" : std::to_string(page) + ".") << '\n';
        }
        const char* type = line.type == ElementType::heading ? "heading" : line.type == ElementType::action ? "action" :
                           line.type == ElementType::cue ? "cue" : line.type == ElementType::parenthetical ? "paren" :
                           line.type == ElementType::more ? "more" : line.type == ElementType::lyric ? "lyric" :
                           line.type == ElementType::transition ? "transition" : line.type == ElementType::centered ? "centered" :
                           line.type == ElementType::title ? "title@center" : line.type == ElementType::credit ? "credit@center" :
                           line.type == ElementType::author ? "author@center" : line.type == ElementType::source ? "source@center" :
                           line.type == ElementType::contact ? "contact@5.2in" : line.type == ElementType::draft_date ? "draft-date@1.5in" : "dialogue";
        actual << line.slot << '\t';
        if (line.column) actual << "dual-";
        actual << type;
        if (line.column) actual << '-' << line.column;
        if (line.continuation && line.type == ElementType::parenthetical) actual << '+';
        actual << '\t' << line.text;
        if (!line.styles.empty()) {
          actual << '\t';
          for (std::size_t i = 0; i < line.styles.size(); ++i) {
            if (i) actual << ' ';
            const auto& style = line.styles[i];
            actual << (style.style == apollo::Style::italic ? "italic=" : style.style == apollo::Style::bold ? "bold=" : "underline=");
            if (line.type == ElementType::lyric && style.start == 0 && style.end == line.text.size()) actual << "all";
            else actual << line.text.substr(style.start, style.end - style.start);
          }
        }
        actual << '\n';
      }
    } catch (const std::exception& e) { std::cerr << name << ": " << e.what() << '\n'; ++failures; continue; }
    std::ostringstream expected_fixture;
    for (std::string line; std::getline(reference, line);) {
      if (!line.empty() && line[0] != '#') expected_fixture << line << '\n';
    }
    if (actual.str() != expected_fixture.str()) {
      std::cerr << name << " does not match its independent expected output\n" << actual.str(); ++failures;
    }
  }
  // With four slots left, this five-line paragraph has no sentence break and
  // must move whole instead of being split arbitrarily.
  std::vector<apollo::Element> push = {{ElementType::heading, "INT. ROOM - NIGHT"}};
  for (int i = 0; i < 25; ++i) push.push_back({ElementType::action, "Filler."});
  push.push_back({ElementType::action, std::string(250, 'x')});
  const auto pushed = apollo::layout(push);
  if (pushed.size() != 31 || pushed[26].page != 2 || pushed[26].slot != 1 ||
      pushed.back().page != 2 || pushed.back().slot != 5) {
    std::cerr << "unsplittable action should move whole\n"; ++failures;
  }
  auto quoted = push;
  quoted.back().text = std::string(65, 'x') + ".\"  " + std::string(130, 'y');
  const auto split = apollo::layout(quoted);
  if (split.size() != 31 || split[27].page != 1 || split[27].slot != 54 ||
      split[27].text != "xxxx.\"" || split[28].page != 2 || split[28].slot != 1 ||
      split[28].text != std::string(61, 'y')) {
    std::cerr << "quoted sentence with extra space should split and rewrap\n"; ++failures;
  }
  auto transition = push;
  transition.back().text = std::string(130, 'x');
  transition.push_back({ElementType::transition, "CUT TO:"});
  const auto moved_transition = apollo::layout(transition);
  if (moved_transition.size() != 30 || moved_transition[26].page != 2 || moved_transition[26].slot != 1 ||
      moved_transition.back().type != ElementType::transition || moved_transition.back().slot != 5) {
    std::cerr << "transition should move with preceding action\n"; ++failures;
  }
  // An oversized unsplittable paragraph must fail instead of looping or
  // stranding the preceding heading on another page.
  try {
    (void)apollo::layout({{ElementType::heading, "INT. ROOM - NIGHT"},
                          {ElementType::action, std::string(61 * 57, 'x')}});
    std::cerr << "impossible heading/action accepted\n"; ++failures;
  } catch (const std::runtime_error&) {}
  std::vector<apollo::Element> heading_speech = {{ElementType::heading, "INT. ROOM - NIGHT"}};
  for (int i = 0; i < 25; ++i) heading_speech.push_back({ElementType::action, "Filler."});
  heading_speech.push_back({ElementType::heading, "EXT. ROAD - NIGHT"});
  heading_speech.push_back({ElementType::cue, "MARA"});
  heading_speech.push_back({ElementType::dialogue, "A short line."});
  const auto kept = apollo::layout(heading_speech);
  if (kept[26].page != 2 || kept[26].slot != 1 || kept[27].page != 2 || kept[27].slot != 3) {
    std::cerr << "heading should move with speech\n"; ++failures;
  }
  auto heading_dual = heading_speech;
  heading_dual.push_back({ElementType::cue, "THEO", true});
  heading_dual.push_back({ElementType::dialogue, "Another short line."});
  const auto kept_dual = apollo::layout(heading_dual);
  if (kept_dual[26].page != 2 || kept_dual[26].slot != 1 || kept_dual[27].column != 'L' ||
      kept_dual[28].column != 'R' || kept_dual[28].slot != 3) {
    std::cerr << "heading should move with dual dialogue\n"; ++failures;
  }
  const auto imported = apollo::import_fountain("INT. ROOM - DAY\r\n\r\nMARA\r\nThis is one\r\ncontinuous sentence.\r\n\r\n===\r\n");
  if (imported.size() != 4 || imported[2].type != ElementType::dialogue ||
      imported[2].text != "This is one continuous sentence." || imported[3].type != ElementType::page_break) {
    std::cerr << "Fountain multiline speech and manual break failed\n"; ++failures;
  }
  for (const std::string source : {"Text [[note]]\n"}) {
    try { (void)apollo::import_fountain(source); std::cerr << "unsupported Fountain accepted\n"; ++failures; }
    catch (const std::runtime_error&) {}
  }
  const auto contd = apollo::layout(apollo::import_fountain("INT. ROOM - DAY\n\nMARA\nHello.\n\nShe turns.\n\nMARA (CONT'D)\nAgain.\n"));
  if (contd.size() != 6 || contd[4].text != "MARA (CONT’D)") {
    std::cerr << "literal continuation must not be doubled\n"; ++failures;
  }
  const auto styled_lines = apollo::layout(apollo::import_fountain(
      "INT. ROOM - NIGHT\n\nMARA\n*This sentence contains enough words to wrap over two dialogue lines.*\n"));
  int italic_lines = 0;
  for (const auto& line : styled_lines) if (line.type == ElementType::dialogue) {
    if (line.styles.size() != 1 || line.styles[0].style != apollo::Style::italic ||
        line.styles[0].start != 0 || line.styles[0].end != line.text.size()) {
      std::cerr << "emphasis across wrapped lines lost\n"; ++failures;
    }
    ++italic_lines;
  }
  if (italic_lines != 2) { std::cerr << "emphasis wrapping fixture failed\n"; ++failures; }
  const auto styled_dual = apollo::layout(apollo::import_fountain(
      "INT. ROOM - NIGHT\n\nMARA\n*Left side.*\n\nTHEO ^\n**Right side.**\n"));
  bool left_style = false, right_style = false;
  for (const auto& line : styled_dual) if (line.type == ElementType::dialogue && line.styles.size() == 1) {
    left_style |= line.column == 'L' && line.styles[0].style == apollo::Style::italic;
    right_style |= line.column == 'R' && line.styles[0].style == apollo::Style::bold;
  }
  if (!left_style || !right_style) { std::cerr << "dual dialogue emphasis lost\n"; ++failures; }
  std::vector<apollo::Element> dual_transition = {{ElementType::heading, "INT. ROOM - NIGHT"}};
  for (int i = 0; i < 25; ++i) dual_transition.push_back({ElementType::action, "Filler."});
  dual_transition.push_back({ElementType::cue, "MARA"});
  dual_transition.push_back({ElementType::dialogue, "This dialogue is long enough for two lines."});
  dual_transition.push_back({ElementType::cue, "THEO", true});
  dual_transition.push_back({ElementType::dialogue, "This dialogue is long enough for two lines."});
  dual_transition.push_back({ElementType::transition, "CUT TO:"});
  const auto moved_dual = apollo::layout(dual_transition);
  if (moved_dual[26].page != 2 || moved_dual[26].column != 'L' || moved_dual[27].column != 'R' ||
      moved_dual.back().type != ElementType::transition || moved_dual.back().slot != 5) {
    std::cerr << "transition should move with the complete dual dialogue\n"; ++failures;
  }
  const auto ending = apollo::layout(apollo::import_fountain(
      "A lamp goes dark.\n\n> FADE OUT.\n\n> THE END <\n"));
  if (ending.size() != 3 || ending[1].slot != 3 || ending[2].slot != 6) {
    std::cerr << "centered text needs two blank slots after a transition\n"; ++failures;
  }
  const auto ending_at_break = apollo::layout({
      {ElementType::action, std::string(61 * 52, 'x')},
      {ElementType::transition, "FADE OUT."},
      {ElementType::centered, "THE END"},
  });
  if (ending_at_break.size() != 54 || ending_at_break[52].slot != 54 ||
      ending_at_break.back().page != 2 || ending_at_break.back().slot != 1) {
    std::cerr << "centered transition gap must count toward fit and drop at page top\n"; ++failures;
  }
  // A heading at slot 54 has only one usable slot for the next element.
  // Three unsplittable lines must take the heading with them to the next page.
  for (const auto type : {ElementType::centered, ElementType::lyric}) {
    auto heading_block = heading_speech;
    heading_block.resize(27);
    heading_block.push_back({type, std::string(type == ElementType::centered ? 121 : 71, 'x')});
    const auto block = apollo::layout(heading_block);
    if (block.size() != 30 || block[26].page != 2 || block[26].slot != 1 ||
        block[27].page != 2 || block[27].slot != 3 || block.back().slot != 5) {
      std::cerr << "heading should move with centered text or lyrics\n"; ++failures;
    }
    heading_block.back().text = "One line.";
    const auto exact_fit = apollo::layout(heading_block);
    if (exact_fit.size() != 28 || exact_fit[26].page != 1 || exact_fit[26].slot != 54 ||
        exact_fit.back().page != 1 || exact_fit.back().slot != 56) {
      std::cerr << "heading and one unsplittable line should fit exactly\n"; ++failures;
    }
  }
  // Editor source mapping. Expected element indexes and UTF-8 byte offsets
  // are counted by hand from the fixture text; none means generated text.
  // Expectations are listed in line order and matched in that order.
  struct Mapped { std::string text; std::size_t source, offset, length; };
  constexpr auto none = apollo::no_source;
  auto check_mapping = [&](const char* name, const std::vector<apollo::Line>& lines, const std::vector<Mapped>& expected) {
    auto from = lines.begin();
    for (const auto& want : expected) {
      const auto found = std::find_if(from, lines.end(), [&](const auto& line) { return line.text == want.text; });
      if (found != lines.end()) from = found + 1;
      if (found == lines.end() || found->source != want.source ||
          (want.source != none && (found->source_offset != want.offset || found->source_length != want.length))) {
        std::cerr << name << " source mapping failed for " << want.text << '\n'; ++failures;
      }
    }
  };
  auto fixture = [](const std::string& name) {
    std::ifstream input(std::string(APOLLO_FIXTURE_DIR) + "/" + name + ".fountain");
    std::ostringstream source; source << input.rdbuf();
    return apollo::import_fountain(source.str());
  };
  check_mapping("dialogue split", apollo::layout(fixture("02-dialogue-split")), {
      {"MARA (V.O.)", 26, 0, 11}, {"We should have left before the tide", 27, 0, 35}, {"turned.", 27, 36, 7},
      {"(MORE)", none, 0, 0}, {"MARA (V.O.) (CONT’D)", none, 0, 0},
      {"Now the road is under water and the", 27, 44, 35}, {"radio is dead. Nobody is coming", 27, 80, 31},
      {"tonight.", 27, 112, 8}, {"Theo does not answer.", 28, 0, 21}});
  check_mapping("continued and dual cues", apollo::layout(fixture("08-dual-dialogue-and-contd")), {
      {"MARA", 1, 0, 4}, {"MARA (CONT’D)", 4, 0, 4}, {"THEO", 6, 0, 4}, {"MARA", 8, 0, 4},
      {"I am holding it. You are the", 9, 0, 28}, {"and stop arguing with me.", 7, 28, 25},
      {"one who keeps shaking.", 9, 29, 22}, {"JUNE", 30, 0, 4}, {"ODELL", 32, 0, 5}});
  check_mapping("dual speech parts", apollo::layout(apollo::import_fountain(
      "MARA\n(looking all around the room)\nWait\n\nTHEO ^\n(looking all around the room)\nWait\n")), {
      {"MARA", 0, 0, 4}, {"THEO", 3, 0, 4}, {"(looking all", 1, 0, 12}, {"(looking all", 4, 0, 12},
      {"around the room)", 1, 13, 16}, {"around the room)", 4, 13, 16}, {"Wait", 2, 0, 4}, {"Wait", 5, 0, 4}});
  check_mapping("transition retry", moved_transition, {
      {std::string(61, 'x'), 26, 0, 61}, {std::string(61, 'x'), 26, 61, 61}, {std::string(8, 'x'), 26, 122, 8},
      {"CUT TO:", 27, 0, 7}});
  // Every non-space source byte appears exactly once, and mapped text matches.
  for (const std::string name : {"01-element-geometry", "02-dialogue-split", "03-heading-keep-with-next", "04-dialogue-push", "05-parenthetical-at-break", "06-action-split", "07-title-page-and-manual-break", "08-dual-dialogue-and-contd"}) {
    const auto elements = fixture(name);
    std::vector<std::string> covered;
    for (const auto& element : elements) covered.emplace_back(element.text.size(), '\0');
    for (const auto& line : apollo::layout(elements)) {
      if (line.source == none) continue;
      const auto& text = elements.at(line.source).text;
      auto shown = text.substr(line.source_offset, line.source_length);
      if (line.type == ElementType::title) for (auto& c : shown) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      if (line.source_offset + line.source_length > text.size() || line.text.compare(0, line.source_length, shown) != 0) {
        std::cerr << name << " maps wrong source text for " << line.text << '\n'; ++failures; continue;
      }
      for (auto i = line.source_offset; i < line.source_offset + line.source_length; ++i) ++covered[line.source][i];
    }
    for (std::size_t e = 0; e < elements.size(); ++e)
      for (std::size_t i = 0; i < elements[e].text.size(); ++i)
        if (covered[e][i] > 1 || (covered[e][i] == 0 && elements[e].text[i] != ' ')) {
          std::cerr << name << " source byte " << e << ':' << i << " is not mapped once\n"; ++failures; break;
        }
  }

  // States an editor passes through. Slots are worked by hand from the
  // profile's spacing table; the rules themselves are provisional (see
  // docs/spike-progress.md): an empty element keeps one blank slot, a cue
  // without speech stands alone, and speech without a cue keeps its spacing.
  struct Placed { int page, slot; std::string text; std::size_t source; char column = 0; };
  auto check_placed = [&](const char* name, const std::vector<apollo::Element>& elements, const std::vector<Placed>& expected) {
    std::vector<apollo::Line> actual;
    try { actual = apollo::layout(elements); }
    catch (const std::exception& error) { std::cerr << name << " threw: " << error.what() << '\n'; ++failures; return; }
    bool same = actual.size() == expected.size();
    for (std::size_t i = 0; same && i < actual.size(); ++i)
      same = actual[i].page == expected[i].page && actual[i].slot == expected[i].slot && actual[i].text == expected[i].text &&
             actual[i].source == expected[i].source && actual[i].column == expected[i].column;
    if (!same) { std::cerr << name << " placement failed\n"; ++failures; }
  };
  check_placed("empty action", {{ElementType::heading, "INT. ROOM - DAY"}, {ElementType::action, ""}, {ElementType::action, "Mara waits."}},
               {{1, 1, "INT. ROOM - DAY", 0}, {1, 3, "", 1}, {1, 5, "Mara waits.", 2}});
  check_placed("empty heading first", {{ElementType::heading, ""}, {ElementType::action, "A lamp."}},
               {{1, 1, "", 0}, {1, 3, "A lamp.", 1}});
  check_placed("empty dialogue", {{ElementType::action, "A lamp."}, {ElementType::cue, "MARA"}, {ElementType::dialogue, ""},
                                  {ElementType::action, "She waits."}},
               {{1, 1, "A lamp.", 0}, {1, 3, "MARA", 1}, {1, 4, "", 2}, {1, 6, "She waits.", 3}});
  check_placed("empty cue and lyric", {{ElementType::action, "A lamp."}, {ElementType::cue, ""}, {ElementType::dialogue, "Hi."},
                                       {ElementType::lyric, ""}},
               {{1, 1, "A lamp.", 0}, {1, 3, "", 1}, {1, 4, "Hi.", 2}, {1, 6, "", 3}});
  check_placed("elements of only spaces", {{ElementType::action, "A lamp."}, {ElementType::action, "  "},
                                           {ElementType::cue, "MARA"}, {ElementType::dialogue, " "},
                                           {ElementType::parenthetical, " "}, {ElementType::action, "End."}},
               {{1, 1, "A lamp.", 0}, {1, 3, "", 1}, {1, 5, "MARA", 2}, {1, 6, "", 3}, {1, 7, "", 4}, {1, 9, "End.", 5}});
  check_placed("cue without speech at end", {{ElementType::action, "A lamp."}, {ElementType::cue, "THEO"}},
               {{1, 1, "A lamp.", 0}, {1, 3, "THEO", 1}});
  check_placed("cue without speech before action", {{ElementType::cue, "THEO"}, {ElementType::action, "He waits."}},
               {{1, 1, "THEO", 0}, {1, 3, "He waits.", 1}});
  check_placed("heading keeps a lone cue", {{ElementType::heading, "INT. ROOM - DAY"}, {ElementType::cue, "THEO"}},
               {{1, 1, "INT. ROOM - DAY", 0}, {1, 3, "THEO", 1}});
  {
    // 55 action lines fill slots 1-55; a lone cue needs a blank slot first.
    std::vector<Placed> expected;
    for (int slot = 1; slot <= 55; ++slot) expected.push_back({1, slot, std::string(61, 'x'), 0});
    expected.push_back({2, 1, "THEO", 1});
    check_placed("lone cue moves to next page", {{ElementType::action, std::string(61 * 55, 'x')}, {ElementType::cue, "THEO"}}, expected);
  }
  check_placed("right cue without left speech", {{ElementType::action, "A lamp."}, {ElementType::cue, "THEO", true},
                                                 {ElementType::dialogue, "Hi."}},
               {{1, 1, "A lamp.", 0}, {1, 3, "THEO", 1}, {1, 4, "Hi.", 2}});
  check_placed("right cue without right speech", {{ElementType::cue, "MARA"}, {ElementType::dialogue, "Hi."}, {ElementType::cue, "THEO", true}},
               {{1, 1, "MARA", 0}, {1, 2, "Hi.", 1}, {1, 4, "THEO", 2}});
  {
    // The unpaired right cue is ordinary, so B6 moves just its speech with
    // the transition; the 52-line action stays on the preceding page.
    std::vector<Placed> expected;
    for (int slot = 1; slot <= 52; ++slot) expected.push_back({1, slot, std::string(61, 'x'), 0});
    expected.push_back({2, 1, "THEO", 1});
    expected.push_back({2, 2, "Hi.", 2});
    expected.push_back({2, 4, "CUT TO:", 3});
    check_placed("transition after an unpaired right cue",
                 {{ElementType::action, std::string(61 * 52, 'x')}, {ElementType::cue, "THEO", true},
                  {ElementType::dialogue, "Hi."}, {ElementType::transition, "CUT TO:"}}, expected);
  }
  check_placed("speech without a cue", {{ElementType::action, "A lamp."}, {ElementType::parenthetical, "(quietly)"},
                                        {ElementType::dialogue, "Hello."}},
               {{1, 1, "A lamp.", 0}, {1, 3, "(quietly)", 1}, {1, 4, "Hello.", 2}});
  {
    // 54 action lines, then two dialogue lines that need slots 56-57.
    std::vector<Placed> expected;
    for (int slot = 1; slot <= 54; ++slot) expected.push_back({1, slot, std::string(61, 'x'), 0});
    expected.push_back({2, 1, "We should have left before the tide", 1});
    expected.push_back({2, 2, "turned.", 1});
    check_placed("speech without a cue moves whole",
                 {{ElementType::action, std::string(61 * 54, 'x')}, {ElementType::dialogue, "We should have left before the tide turned."}},
                 expected);
  }
  // A cue runs from 4.0 in to 7.6 in, so it wraps at 36 columns. Its lines
  // count toward fit, and a generated suffix may take a line of its own.
  const std::string long_cue = "MARA AND THEO AND JUNE AND ODELL AND";
  check_placed("long cue wraps", {{ElementType::cue, long_cue + " THE KEEPER"}, {ElementType::dialogue, "Hi."}},
               {{1, 1, long_cue, 0}, {1, 2, "THE KEEPER", 0}, {1, 3, "Hi.", 1}});
  {
    // 51 action slots leave a heading at 54 and one cue slot at 56. A
    // two-line cue without speech needs the heading to move with it.
    std::vector<Placed> expected;
    for (int slot = 1; slot <= 51; ++slot) expected.push_back({1, slot, std::string(61, 'x'), 0});
    expected.push_back({2, 1, "INT. ROOM - DAY", 1});
    expected.push_back({2, 3, long_cue, 2});
    expected.push_back({2, 4, "THE KEEPER", 2});
    check_placed("heading keeps a wrapped lone cue",
                 {{ElementType::action, std::string(61 * 51, 'x')}, {ElementType::heading, "INT. ROOM - DAY"},
                  {ElementType::cue, long_cue + " THE KEEPER"}}, expected);
    try {
      (void)apollo::layout({{ElementType::cue, std::string(36 * 57, 'x')}});
      std::cerr << "lone cue exceeding a page accepted\n"; ++failures;
    } catch (const std::runtime_error&) {}
  }
  {
    const auto wrapped = apollo::layout({{ElementType::cue, long_cue + " THE KEEPER"}, {ElementType::dialogue, "Hi."}});
    if (wrapped.size() != 3 || wrapped[1].source_offset != 37 || wrapped[1].source_length != 10 || wrapped[1].geometry.x != 288) {
      std::cerr << "wrapped cue mapping failed\n"; ++failures;
    }
    std::vector<Placed> expected;
    for (int slot = 1; slot <= 54; ++slot) expected.push_back({1, slot, std::string(61, 'x'), 0});
    expected.push_back({2, 1, long_cue, 1});
    expected.push_back({2, 2, "THE KEEPER", 1});
    expected.push_back({2, 3, "Hi.", 2});
    check_placed("long cue moves with its speech", {{ElementType::action, std::string(61 * 54, 'x')},
                 {ElementType::cue, long_cue + " THE KEEPER"}, {ElementType::dialogue, "Hi."}}, expected);
  }
  check_placed("continuation suffix on its own line",
               {{ElementType::cue, long_cue}, {ElementType::dialogue, "Hi."}, {ElementType::action, "They wait."},
                {ElementType::cue, long_cue}, {ElementType::dialogue, "Again."}},
               {{1, 1, long_cue, 0}, {1, 2, "Hi.", 1}, {1, 4, "They wait.", 2}, {1, 6, long_cue, 3},
                {1, 7, "(CONT’D)", none}, {1, 8, "Again.", 4}});
  {
    // A typed suffix replaced by a generated one that wraps: the typed bytes
    // are reported on the last line that shows source text.
    const auto shown = apollo::layout({{ElementType::cue, long_cue}, {ElementType::dialogue, "Hi."}, {ElementType::action, "They wait."},
                                       {ElementType::cue, long_cue + " (CONT'D)"}, {ElementType::dialogue, "Again."}});
    if (shown.size() != 6 || shown[3].text != long_cue || shown[3].source_length != 36 || shown[3].replaced_length != 9 ||
        shown[4].text != "(CONT’D)" || shown[4].source != none) {
      std::cerr << "wrapped typed continuation mapping failed\n"; ++failures;
    }
  }
  // A trailing space, as left by Enter after a space, is not text: it does
  // not move right-aligned or centered lines.
  {
    const auto aligned = apollo::layout({{ElementType::transition, "CUT TO: "}, {ElementType::centered, "THE END "}});
    if (aligned.size() != 2 || aligned[0].text != "CUT TO:" || aligned[0].geometry.x != 511.2 - 7 * apollo::profile::advance ||
        aligned[1].text != "THE END" || aligned[1].geometry.x != 324 - 7 * apollo::profile::advance / 2) {
      std::cerr << "trailing space moved an aligned line\n"; ++failures;
    }
  }
  // A right dual cue runs from 5.6 in to 7.6 in: 20 columns, as on the left.
  check_placed("dual cue wraps in its column",
               {{ElementType::cue, "MARA"}, {ElementType::dialogue, "Hold it."},
                {ElementType::cue, "ODELL AND THE KEEPER ARE LATE", true}, {ElementType::dialogue, "Wait."}},
               {{1, 1, "MARA", 0, 'L'}, {1, 1, "ODELL AND THE KEEPER", 2, 'R'}, {1, 2, "Hold it.", 1, 'L'},
                {1, 2, "ARE LATE", 2, 'R'}, {1, 3, "Wait.", 3, 'R'}});
  // Cues and headings are shown uppercase; the source keeps what was typed,
  // and the whole typed text stays mapped.
  {
    const auto shown = apollo::layout({{ElementType::heading, "int. room - day"}, {ElementType::cue, "mara (v.o.)"},
                                       {ElementType::dialogue, "Hi."}, {ElementType::action, "She turns."},
                                       {ElementType::cue, "Mara"}, {ElementType::dialogue, "Again."}});
    if (shown.size() != 6 || shown[0].text != "INT. ROOM - DAY" || shown[0].source_length != 15 ||
        shown[1].text != "MARA (V.O.)" || shown[1].source_length != 11 || shown[2].text != "Hi." ||
        shown[4].text != "MARA (CONT’D)" || shown[4].source_length != 4 || shown[4].replaced_length != 0) {
      std::cerr << "cues and headings should be shown uppercase\n"; ++failures;
    }
  }
  // A typed (CONT'D) that G1 would generate is shown as the generated form,
  // never doubled. The typed suffix is reported as replaced source text so an
  // editor can hide it under the generated suffix.
  for (const std::string literal : {" (CONT'D)", " (CONT’D)", " (cont'd)"}) {
    const auto shown = apollo::layout({{ElementType::cue, "MARA"}, {ElementType::dialogue, "Hello."},
                                       {ElementType::action, "She turns."}, {ElementType::cue, "MARA" + literal},
                                       {ElementType::dialogue, "Again."}});
    if (shown.size() != 5 || shown[3].text != "MARA (CONT’D)" || shown[3].source != 3 || shown[3].source_length != 4 ||
        shown[3].replaced_length != literal.size()) {
      std::cerr << "typed continuation" << literal << " should be replaced by the generated form\n"; ++failures;
    }
  }
  return failures ? 1 : 0;
}
