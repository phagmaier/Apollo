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
struct StyleSpan { std::size_t start; std::size_t end; Style style; };
struct Element { ElementType type; std::string text; bool dual_right = false; std::vector<StyleSpan> styles = {}; };
struct Line { int page; int slot; ElementType type; std::string text; char column = 0;
              bool continuation = false; std::vector<StyleSpan> styles = {}; TextGeometry geometry = {}; };
struct PositionedText { std::string text; TextGeometry geometry; };
struct Page { int body_number; std::optional<PositionedText> number; };
struct DocumentLayout { std::vector<Line> lines; std::vector<Page> pages; };

// Coordinates are points from the physical page's top left. Title page = 0.
DocumentLayout layout_document(const std::vector<Element>& elements);
// Convenience line view for fixture tools and existing engine callers.
std::vector<Line> layout(const std::vector<Element>& elements);
}
