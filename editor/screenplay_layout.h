#pragma once

#include "engine/layout.h"

#include <QAbstractListModel>
#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QTextFormat>
#include <memory>
#include <vector>

class QTextCursor;

namespace apollo::editor {
// Each QTextBlock is one screenplay element. Its type is a block property.
inline constexpr int element_type_property = QTextFormat::UserProperty + 1;
inline constexpr int dual_right_property = QTextFormat::UserProperty + 2;
// The element's persistent id (source/screenplay_format.h), a QString.
inline constexpr int element_id_property = QTextFormat::UserProperty + 4;
// Document coordinates are 96 dpi pixels. At 12 pt = 16 px the Courier Prime
// advance (9.59375 px) is exact in QTextLayout's 1/64 px fixed point; at 12 px
// it rounds to 7.1875 and drifts 0.48 pt across a 61-column line.
inline constexpr double px_per_point = 4.0 / 3;
inline constexpr double page_width = profile::page_width * px_per_point, page_height = profile::page_height * px_per_point;
// Page cards are page_stride apart. A title page, when present, is the first
// card and body page n is card n; otherwise body page n is card n - 1.
inline constexpr double page_gap = 32, page_stride = page_height + page_gap;

// Registers the bundled Courier Prime faces and returns the 12 pt body font.
QFont courier_prime(const QString& font_directory);
// One block per element, with emphasis as character formats, laid out by a
// ScreenplayLayout. The layout is installed before the document is returned
// because an editor connects to the layout when it receives the document.
// A deliberate line break ('\n' in element text) is a QChar::LineSeparator in
// the block. Elements without an id get a new one.
std::unique_ptr<QTextDocument> screenplay_document(const std::vector<Element>& elements, const QFont& font);
std::vector<Element> document_elements(const QTextDocument& document);
ElementType element_type(const QTextBlock& block);
Element block_element(const QTextBlock& block);
// QTextBlock text (or a prefix) as UTF-8 source text: internal U+2028
// separators become '\n'. Use this mapping for both text and byte offsets.
std::string element_text(QString text);
// Splits an element at a UTF-8 byte offset, keeping emphasis on both parts.
// The first part keeps the id unless it is empty and the second is not; the
// part without the id has none.
std::pair<Element, Element> split_element(const Element& element, std::size_t byte);
// Inserts an element's text with its emphasis at the cursor.
void insert_runs(QTextCursor& cursor, const Element& element);
// Writes elements from the cursor's empty block, one block per element,
// giving a new id to each element without one.
void insert_elements(QTextCursor& cursor, const std::vector<Element>& elements);
// Gives a new id to each block that has none or repeats an earlier block's,
// such as a block Qt split without an editor command. Used at initialization
// and synchronously on undoCommandAdded, joining the originating edit.
// Never call while saving or replaying undo/redo. Returns the repair count.
int repair_identities(QTextDocument& document);

// Generated text: (MORE), continuation cues and suffixes, and page numbers.
// kind uses the fixture element names, such as "more" or "cue".
struct Decoration { QString text; QPointF baseline; QString kind; std::vector<StyleSpan> styles; };

// Decorations split into uniformly styled runs for QML Text items. Updates
// touch only the rows that changed, so typing does not recreate every item.
class DecorationModel : public QAbstractListModel {
  Q_OBJECT
public:
  struct Run {
    QString text; double x, baseline; bool bold, italic, underline;
    bool operator==(const Run&) const = default;
  };
  enum Role { TextRole = Qt::UserRole + 1, XRole, BaselineRole, BoldRole, ItalicRole, UnderlineRole };
  using QAbstractListModel::QAbstractListModel;
  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  void replace(std::vector<Run> runs);
  const std::vector<Run>& runs() const { return runs_; }

private:
  std::vector<Run> runs_;
};

// A line as the editor shows it, read back from the laid-out QTextLines and
// decorations: page (0 = title page), body slot, fixture element name, left
// edge in points, and visible text. Generated suffixes join their line.
struct ShownLine { int page; int slot; QString kind; double x; QString text; };
std::vector<ShownLine> shown_lines(const QTextDocument& document);

// Positions each block's QTextLines at the engine's line breaks and point
// geometry. Generated text is exposed as decorations because it is not
// document text. The engine runs on the whole document after each change;
// only blocks whose breaks or relative positions changed are laid out again.
class ScreenplayLayout : public QAbstractTextDocumentLayout {
  Q_OBJECT
  Q_PROPERTY(int pageCount READ pageCount NOTIFY layoutChanged)
  Q_PROPERTY(qreal pageWidth READ pageWidth CONSTANT)
  Q_PROPERTY(qreal pageHeight READ pageHeight CONSTANT)
  Q_PROPERTY(qreal pageGap READ pageGap CONSTANT)
  Q_PROPERTY(qreal documentHeight READ documentHeight NOTIFY layoutChanged)
  Q_PROPERTY(QFont font READ font CONSTANT)
  Q_PROPERTY(QObject* decorations READ decoration_model CONSTANT)
  Q_PROPERTY(QString error READ error NOTIFY layoutChanged)
public:
  explicit ScreenplayLayout(QTextDocument* document);
  void draw(QPainter* painter, const PaintContext& context) override;
  int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;
  // Page cards, including the title page when there is one.
  int pageCount() const override { return pages_; }
  QSizeF documentSize() const override;
  QRectF frameBoundingRect(QTextFrame* frame) const override;
  QRectF blockBoundingRect(const QTextBlock& block) const override;

  qreal pageWidth() const { return page_width; }
  qreal pageHeight() const { return page_height; }
  qreal pageGap() const { return page_gap; }
  qreal documentHeight() const { return documentSize().height(); }
  QFont font() const;
  QObject* decoration_model() { return &model_; }
  const std::vector<Decoration>& decoration_list() const { return decorations_; }
  bool has_title_page() const { return title_page_; }
  // The top of a page in document pixels (page 0 is the title page).
  double page_top(int page) const { return (page - (title_page_ ? 0 : 1)) * page_stride; }
  // The engine's failure for the current text, or empty when it laid out.
  QString error() const { return error_; }
  // Total time spent laying the document out, and blocks whose lines were
  // rebuilt by the last layout, for measurements and tests.
  qint64 relayout_nanoseconds() const { return relayout_nanoseconds_; }
  int rebuilt_blocks() const { return rebuilt_blocks_; }

signals:
  void layoutChanged();

protected:
  void documentChanged(int from, int removed, int added) override;

private:
  void relayout();
  void fallback_layout();
  std::vector<QRectF> block_rects_;
  std::vector<Decoration> decorations_;
  DecorationModel model_;
  int pages_ = 1;
  bool title_page_ = false;
  QString error_;
  qint64 relayout_nanoseconds_ = 0;
  int rebuilt_blocks_ = 0;
};
}
