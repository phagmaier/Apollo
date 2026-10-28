#include "editor/editor_view.h"
#include "editor/editor_commands.h"
#include "editor/screenplay_layout.h"

#include <QQuickTextDocument>
#include <stdexcept>

namespace apollo::editor {
QQuickItem* show_editor(QQuickView& view, QTextDocument& document) {
  auto* layout = qobject_cast<ScreenplayLayout*>(document.documentLayout());
  if (!layout) throw std::runtime_error("document does not use ScreenplayLayout");
  auto* commands = new EditorCommands(&document, &view);
  view.setInitialProperties({{"screenplayLayout", QVariant::fromValue<QObject*>(layout)},
                             {"commands", QVariant::fromValue<QObject*>(commands)}});
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
