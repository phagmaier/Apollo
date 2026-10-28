#include "engine/wrap.h"

#include <stdexcept>

namespace apollo {
std::vector<std::string> wrap(std::string_view text, std::size_t width) {
  if (width == 0) throw std::invalid_argument("wrap width must be positive");
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < text.size()) {
    while (start < text.size() && text[start] == ' ') ++start;
    if (start == text.size()) break;
    std::size_t end = start, columns = 0, last_space = std::string_view::npos;
    while (end < text.size() && columns < width) {
      if (text[end] == ' ') last_space = end;
      ++end;
      while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) ++end;
      ++columns;
    }
    if (end == text.size()) {
      lines.emplace_back(text.substr(start));
      break;
    }
    if (text[end] == ' ') {
      lines.emplace_back(text.substr(start, end - start));
      start = end + 1;
    } else if (last_space != std::string_view::npos) {
      lines.emplace_back(text.substr(start, last_space - start));
      start = last_space + 1;
    } else {
      lines.emplace_back(text.substr(start, end - start));
      start = end;
    }
  }
  return lines;
}
}
