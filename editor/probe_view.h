#pragma once

#include <QQuickItem>
#include <QQuickView>
#include <QTextDocument>

namespace apollo::editor {
// Loads TextEditProbe.qml and gives its TextEdit the document, which must
// already use a ScreenplayLayout. Returns the TextEdit; throws on failure.
QQuickItem* show_probe(QQuickView& view, QTextDocument& document);
}
