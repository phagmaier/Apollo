#pragma once
#include "engine/layout.h"
#include <string_view>
#include <vector>

namespace apollo {
// Parse the supported subset or throw when an unsupported construct is found.
std::vector<Element> import_fountain(std::string_view source);
}
