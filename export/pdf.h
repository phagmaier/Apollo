#pragma once

#include "engine/layout.h"

#include <QString>

namespace apollo {
// Requires QGuiApplication on the calling thread. Throws on export failure;
// an existing destination is replaced only after successful PDF finalization.
void export_pdf(const DocumentLayout& document, const QString& destination,
                const QString& font_directory, bool include_title = true);
}
