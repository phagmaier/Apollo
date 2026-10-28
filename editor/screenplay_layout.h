#pragma once

#include "engine/layout.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QTextFormat>
#include <QVariantList>
#include <memory>
#include <vector>

namespace apollo::editor {
// Each QTextBlock is one screenplay element. Its type is a block property.
inline constexpr int element_type_property = QTextFormat::UserProperty + 1;
inline constexpr int dual_right_property = QTextFormat::UserProperty + 2;
// Document coordinates are 96 dpi pixels. At 12 pt = 16 px the Courier Prime
// advance (9.59375 px) is exact in QTextLayout's 1/64 px fixed point; at 12 px
// it rounds to 7.1875 and drifts 0.48 pt across a 61-column line.
inline constexpr double px_per_point = 4.0 / 3;
inline constexpr double page_width = profile::page_width * px_per_point, page_height = profile::page_height * px_per_point;
// Body page n starts at (n - 1) * page_stride.
inline constexpr double page_gap = 32, page_stride = page_height + page_gap;

// Registers the bundled Courier Prime faces and returns the 12 pt body font.
QFont courier_prime(const QString& font_directory);
// One block per element, with emphasis as character formats, laid out by a
// ScreenplayLayout. The layout is installed before the document is returned
// because an editor connects to the layout when it receives the document.
std::unique_ptr<QTextDocument> screenplay_document(const std::vector<Element>& elements, const QFont& font);
std::vector<Element> document_elements(const QTextDocument& document);

// Positions each block's QTextLines at the engine's line breaks and point
// geometry. Generated text (MORE, CONT'D, page numbers) is exposed as
// decorations because it is not document text. The whole document is laid
// out again on each change.
class ScreenplayLayout : public QAbstractTextDocumentLayout {
  Q_OBJECT
  Q_PROPERTY(int pageCount READ pageCount NOTIFY layoutChanged)
  Q_PROPERTY(qreal pageWidth READ pageWidth CONSTANT)
  Q_PROPERTY(qreal pageHeight READ pageHeight CONSTANT)
  Q_PROPERTY(qreal pageGap READ pageGap CONSTANT)
  Q_PROPERTY(qreal documentHeight READ documentHeight NOTIFY layoutChanged)
  Q_PROPERTY(QFont font READ font CONSTANT)
  Q_PROPERTY(QVariantList decorations READ decorations NOTIFY layoutChanged)
  Q_PROPERTY(QString error READ error NOTIFY layoutChanged)
public:
  struct Decoration { QString text; QPointF baseline; };

  explicit ScreenplayLayout(QTextDocument* document);
  void draw(QPainter* painter, const PaintContext& context) override;
  int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;
  int pageCount() const override { return pages_; }
  QSizeF documentSize() const override;
  QRectF frameBoundingRect(QTextFrame* frame) const override;
  QRectF blockBoundingRect(const QTextBlock& block) const override;

  qreal pageWidth() const { return page_width; }
  qreal pageHeight() const { return page_height; }
  qreal pageGap() const { return page_gap; }
  qreal documentHeight() const { return documentSize().height(); }
  QFont font() const;
  QVariantList decorations() const;
  const std::vector<Decoration>& decoration_list() const { return decorations_; }
  // The engine's failure for the current text, or empty when it laid out.
  QString error() const { return error_; }

signals:
  void layoutChanged();

protected:
  void documentChanged(int from, int removed, int added) override;

private:
  void relayout();
  void fallback_layout();
  std::vector<QRectF> block_rects_;
  std::vector<Decoration> decorations_;
  int pages_ = 1;
  QString error_;
};
}
