#pragma once

#include <QQuickItem>
#include <QQuickView>
#include <QTextDocument>

namespace apollo::editor {
class Autosaver;
class PdfExportJob;
class FountainExportJob;
// Loads the Writing desk shell and gives its TextEdit the document, which must
// already use a ScreenplayLayout, with EditorCommands owned by the view. The
// shell shows the autosaver's status, or that edits are not saved without one.
// Without a Fountain job the desk offers only PDF. Returns the TextEdit;
// throws on failure.
QQuickItem* show_editor(QQuickView& view, QTextDocument& document, PdfExportJob* export_job = nullptr,
                        const QString& export_destination = {}, Autosaver* autosaver = nullptr,
                        FountainExportJob* fountain_job = nullptr);
}
