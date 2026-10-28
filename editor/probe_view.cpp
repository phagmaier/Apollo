#include "editor/probe_view.h"
#include "editor/screenplay_layout.h"

#include <QQuickTextDocument>
#include <stdexcept>

namespace apollo::editor {
QQuickItem* show_probe(QQuickView& view, QTextDocument& document) {
  auto* layout = qobject_cast<ScreenplayLayout*>(document.documentLayout());
  if (!layout) throw std::runtime_error("document does not use ScreenplayLayout");
  view.setInitialProperties({{"screenplayLayout", QVariant::fromValue<QObject*>(layout)}});
  view.setResizeMode(QQuickView::SizeRootObjectToView);
  view.setSource(QUrl::fromLocalFile(QStringLiteral(APOLLO_EDITOR_QML)));
  if (view.status() != QQuickView::Ready) {
    QString errors;
    for (const auto& error : view.errors()) errors += error.toString() + '\n';
    throw std::runtime_error("cannot load probe QML: " + errors.toStdString());
  }
  auto* editor = view.rootObject()->findChild<QQuickItem*>(QStringLiteral("editor"));
  auto* handle = editor ? editor->property("textDocument").value<QQuickTextDocument*>() : nullptr;
  if (!handle) throw std::runtime_error("probe QML has no TextEdit document");
  // Public since Qt 6.7. The layout must be installed first: TextEdit's text
  // control connects to the document's layout when the document is set.
  handle->setTextDocument(&document);
  return editor;
}
}
