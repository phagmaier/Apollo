#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace apollo {
// Wrap visible UTF-8 text by character columns. Spaces are break points and
// never end a line.
std::vector<std::string> wrap(std::string_view text, std::size_t width);
}
