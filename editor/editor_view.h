#pragma once

#include <QQuickItem>
#include <QQuickView>
#include <QTextDocument>

namespace apollo::editor {
class PdfExportJob;
// Loads ScreenplayEditor.qml and gives its TextEdit the document, which must
// already use a ScreenplayLayout, with EditorCommands owned by the view.
// Returns the TextEdit; throws on failure.
QQuickItem* show_editor(QQuickView& view, QTextDocument& document, PdfExportJob* export_job = nullptr,
                        const QString& export_destination = {});
}
