#pragma once

#include "source/project.h"

namespace apollo::source {
// Documents/Apollo, or home/Apollo when no Documents location is available.
QString default_project_location();

// Creates a complete blank project with one empty action at revision zero.
// parent must be a clean absolute existing directory; only the default location
// may be created. Existing parent aliases are resolved canonically. Trims name
// and chooses name, name (2), ... without replacing any existing entry. At most
// 1000 candidates are considered. Throws SourceError on invalid input or I/O.
Project create_blank_project(const QString& parent, const QString& name);
}
