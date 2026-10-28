#pragma once

#include <string>
#include <vector>

namespace apollo {
enum class ElementType { heading, action, cue, parenthetical, dialogue, page_break, more,
                         lyric, transition, centered, title, credit, author, source, contact, draft_date };
enum class Style { italic, bold, underline };
// Offsets are UTF-8 byte offsets into the associated text, with an exclusive end.
struct StyleSpan { std::size_t start; std::size_t end; Style style; };
struct Element { ElementType type; std::string text; bool dual_right = false; std::vector<StyleSpan> styles = {}; };
struct Line { int page; int slot; ElementType type; std::string text; char column = 0;
              bool continuation = false; std::vector<StyleSpan> styles = {}; };

// Initial vertical-layout slice; this is not yet the complete export profile.
std::vector<Line> layout(const std::vector<Element>& elements);
}
