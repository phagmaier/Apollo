#include "engine/layout.h"
#include "fountain/import.h"

#include <cmath>
#include <iostream>

int main() {
  using namespace apollo;
  int failures = 0;
  auto check = [&](const Line& line, double x, double baseline, double width) {
    if (std::abs(line.geometry.x - x) > 0.00001 ||
        std::abs(line.geometry.baseline - baseline) > 0.00001 ||
        std::abs(line.geometry.width - width) > 0.00001) {
      std::cerr << "wrong point geometry for " << line.text << '\n'; ++failures;
    }
  };
  // Independent worked values from the profile in points. Four visible glyphs
  // have width 28.78125 regardless of UTF-8 byte length or face.
  const auto lines = layout({
      {ElementType::heading, "INT."}, {ElementType::action, "café"},
      {ElementType::cue, "MARA"}, {ElementType::parenthetical, "(looking all around the room)"},
      {ElementType::dialogue, "Wait"}, {ElementType::lyric, "Sing"},
      {ElementType::transition, "CUT TO:"}, {ElementType::centered, "THE END"},
  });
  if (lines.size() != 9) return 1;
  check(lines[0], 108, 81, 28.78125);
  check(lines[1], 108, 105, 28.78125);
  check(lines[2], 288, 129, 28.78125);
  check(lines[3], 244.8, 141, 86.34375); // "(looking all"
  check(lines[4], 252, 153, 115.125);  // "around the room)"
  check(lines[5], 216, 165, 28.78125);
  check(lines[6], 216, 189, 28.78125);
  check(lines[7], 460.8328125, 213, 50.3671875);
  check(lines[8], 298.81640625, 249, 50.3671875);
  const auto dual = layout(import_fountain(
      "MARA\n(looking all around the room)\nWait\n\nTHEO ^\n(looking all around the room)\nWait\n"));
  if (dual.size() != 8) return 1;
  check(dual[0], 180, 81, 28.78125);
  check(dual[1], 403.2, 81, 28.78125);
  check(dual[2], 136.8, 93, 86.34375);
  check(dual[3], 360, 93, 86.34375);
  check(dual[4], 144, 105, 115.125);
  check(dual[5], 367.2, 105, 115.125);
  check(dual[6], 108, 117, 28.78125);
  check(dual[7], 331.2, 117, 28.78125);
  const auto title = layout_document(import_fountain(
      "Title: The Lamp Room\nCredit: Written by\nAuthor: R. Example\n"
      "Contact: 555-0100\nDraft date: Today\n\nA lamp.\n\n===\n\n===\n"));
  if (title.lines.size() != 6 || title.pages.size() != 4) return 1;
  check(title.lines[0], 259.23046875, 117, 93.5390625);
  check(title.lines[1], 270.0234375, 189, 71.953125);
  check(title.lines[2], 270.0234375, 249, 71.953125);
  check(title.lines[3], 374.4, 645, 57.5625);
  check(title.lines[4], 108, 645, 35.9765625);
  if (title.pages[0].body_number != 0 || title.pages[0].number || title.pages[1].number ||
      !title.pages[2].number || title.pages[2].number->text != "2." ||
      title.pages[2].number->geometry.x != 507.609375 ||
      title.pages[2].number->geometry.baseline != 45 ||
      !title.pages[3].number || title.pages[3].number->text != "3.") {
    std::cerr << "title/blank-page numbering geometry failed\n"; ++failures;
  }
  return failures ? 1 : 0;
}
