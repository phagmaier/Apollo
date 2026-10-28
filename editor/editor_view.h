#pragma once

#include <QQuickItem>
#include <QQuickView>
#include <QTextDocument>

namespace apollo::editor {
// Loads ScreenplayEditor.qml and gives its TextEdit the document, which must
// already use a ScreenplayLayout, with EditorCommands owned by the view.
// Returns the TextEdit; throws on failure.
QQuickItem* show_editor(QQuickView& view, QTextDocument& document);
}
