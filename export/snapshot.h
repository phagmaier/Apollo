#pragma once

#include "engine/layout.h"
#include <QByteArray>

namespace apollo {
// Private, versioned worker message. This is not a persisted source format.
struct ExportSnapshot { std::vector<Element> elements; bool include_title = true; };
QByteArray encode_export_snapshot(const ExportSnapshot& snapshot);
ExportSnapshot decode_export_snapshot(const QByteArray& bytes);
}
