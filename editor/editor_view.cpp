#include "editor/editor_view.h"
#include "editor/autosave.h"
#include "editor/editor_commands.h"
#include "editor/screenplay_layout.h"
#include "editor/pdf_export_job.h"
#include "editor/fountain_export_job.h"

#include <QQuickTextDocument>
#include <QFileInfo>
#include <QTextBlock>
#include <stdexcept>

namespace apollo::editor {
QQuickItem* show_editor(QQuickView& view, QTextDocument& document, PdfExportJob* export_job, const QString& export_destination,
                        Autosaver* autosaver, FountainExportJob* fountain_job) {
  auto* layout = qobject_cast<ScreenplayLayout*>(document.documentLayout());
  if (!layout) throw std::runtime_error("document does not use ScreenplayLayout");
  auto* commands = new EditorCommands(&document, &document);
  QString title = export_destination.isEmpty() ? QStringLiteral("Screenplay") : QFileInfo(export_destination).completeBaseName();
  QStringList scenes;
  // Opening snapshot for the bounded visual check, not a scene navigator.
  for (auto block = document.begin(); block != document.end(); block = block.next()) {
    if (element_type(block) == ElementType::heading)
      scenes.push_back(QString::fromStdString(presented_text(ElementType::heading, block.text().toStdString())));
  }
  view.setInitialProperties({{"screenplayLayout", QVariant::fromValue<QObject*>(layout)},
                             {"commands", QVariant::fromValue<QObject*>(commands)},
                             {"exportJob", QVariant::fromValue<QObject*>(export_job)},
                             {"autosaver", QVariant::fromValue<QObject*>(autosaver)},
                             {"exportDestination", export_destination},
                             {"fountainJob", QVariant::fromValue<QObject*>(fountain_job)},
                             {"documentTitle", title}, {"outlineScenes", scenes}});
  view.setResizeMode(QQuickView::SizeRootObjectToView);
  view.setSource(QUrl::fromLocalFile(QStringLiteral(APOLLO_EDITOR_QML)));
  if (view.status() != QQuickView::Ready) {
    QString errors;
    for (const auto& error : view.errors()) errors += error.toString() + '\n';
    throw std::runtime_error("cannot load editor QML: " + errors.toStdString());
  }
  auto* editor = view.rootObject()->findChild<QQuickItem*>(QStringLiteral("editor"));
  auto* handle = editor ? editor->property("textDocument").value<QQuickTextDocument*>() : nullptr;
  if (!handle) throw std::runtime_error("editor QML has no TextEdit document");
  // Public since Qt 6.7. The layout must be installed first: TextEdit's text
  // control connects to the document's layout when the document is set.
  handle->setTextDocument(&document);
  // TextEdit limits text nodes to the visible area only when large text
  // arrives through its text property; a document set directly needs the
  // flag set here, or every keystroke rebuilds nodes for every page.
  editor->setFlag(QQuickItem::ItemObservesViewport);
  return editor;
}
}
