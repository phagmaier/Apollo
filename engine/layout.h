#pragma once

#include <optional>
#include <string>
#include <vector>

namespace apollo {
namespace profile {
inline constexpr double page_width = 612, page_height = 792;
inline constexpr double font_size = 12, line_pitch = 12;
// Pinned Courier Prime: 1228 font units per glyph, 2048 units per em.
inline constexpr double advance = 7.1953125;
}
struct TextGeometry { double x = 0, baseline = 0, width = 0; };
enum class ElementType { heading, action, cue, parenthetical, dialogue, page_break, more,
                         lyric, transition, centered, title, credit, author, source, contact, draft_date };
enum class Style { italic, bold, underline };
// Offsets are UTF-8 byte offsets into the associated text, with an exclusive end.
struct StyleSpan {
  std::size_t start; std::size_t end; Style style;
  bool operator==(const StyleSpan&) const = default;
};
// A '\n' in text is a deliberate line break. The id is the element's
// persistent source identity (source/screenplay_format.h); layout ignores it.
struct Element {
  ElementType type; std::string text; bool dual_right = false; std::vector<StyleSpan> styles = {}; std::string id = {};
  bool operator==(const Element&) const = default;
};
inline constexpr std::size_t no_source = static_cast<std::size_t>(-1);
// source is the input element index. The line's first source_length bytes
// show that element's text from source_offset (titles, cues, and headings are
// shown uppercase). Any rest, such as an automatic " (CONT’D)", is generated.
// The generated rest stands in for the next replaced_length source bytes,
// such as a typed (CONT'D). Generated lines, such as (MORE) and a cue
// repeated after a page break, have no source.
struct Line { int page; int slot; ElementType type; std::string text; char column = 0;
              bool continuation = false; std::vector<StyleSpan> styles = {}; TextGeometry geometry = {};
              std::size_t source = no_source, source_offset = 0, source_length = 0, replaced_length = 0; };
struct PositionedText { std::string text; TextGeometry geometry; };
struct Page { int body_number; std::optional<PositionedText> number; };
struct DocumentLayout { std::vector<Line> lines; std::vector<Page> pages; };

// Coordinates are points from the physical page's top left. Title page = 0.
DocumentLayout layout_document(const std::vector<Element>& elements);
// Convenience line view for fixture tools and existing engine callers.
std::vector<Line> layout(const std::vector<Element>& elements);

// Case presentation (docs/case-handling.md). Element text is authored text: it
// keeps the case that was typed or imported. Scene headings, character cues
// and the title are presented in capitals by converting ASCII a-z only, so byte
// offsets and emphasis spans are unchanged. Any other letter, such as é, is
// presented as authored and therefore not in capitals.
bool presented_in_capitals(ElementType type);
std::string presented_text(ElementType type, std::string text);
}
