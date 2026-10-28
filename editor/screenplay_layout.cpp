#include "editor/screenplay_layout.h"

#include <QDir>
#include <QFontDatabase>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace apollo::editor {
namespace {
constexpr double left_margin = 108 * px_per_point, pitch = profile::line_pitch * px_per_point;

int utf16_index(const std::string& text, std::size_t byte) {
  return static_cast<int>(QString::fromUtf8(text.data(), static_cast<qsizetype>(byte)).size());
}

// QTextLine stores positions in 1/64 px; round rather than let it truncate.
double snap(double px) { return std::round(px * 64) / 64; }

QTextOption line_option() {
  // Lines are broken only where the engine says, including inside a long word.
  QTextOption option(Qt::AlignLeft);
  option.setWrapMode(QTextOption::WrapAnywhere);
  option.setUseDesignMetrics(true);
  return option;
}
}

QFont courier_prime(const QString& font_directory) {
  QString family;
  for (const char* face : {"Regular", "Italic", "Bold", "BoldItalic"}) {
    const int id = QFontDatabase::addApplicationFont(QDir(font_directory).filePath(QString("CourierPrime-%1.ttf").arg(face)));
    const auto families = id < 0 ? QStringList() : QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty()) throw std::runtime_error("cannot register bundled Courier Prime font");
    family = families.front();
  }
  QFont font(family);
  font.setPixelSize(static_cast<int>(std::lround(profile::font_size * px_per_point)));
  font.setKerning(false); font.setHintingPreference(QFont::PreferNoHinting);
  font.setStyleStrategy(QFont::NoFontMerging);
  return font;
}

std::unique_ptr<QTextDocument> screenplay_document(const std::vector<Element>& elements, const QFont& font) {
  auto document = std::make_unique<QTextDocument>();
  document->setDefaultFont(font);
  QTextCursor cursor(document.get());
  for (std::size_t i = 0; i < elements.size(); ++i) {
    const auto& element = elements[i];
    QTextBlockFormat block;
    block.setProperty(element_type_property, static_cast<int>(element.type));
    if (element.dual_right) block.setProperty(dual_right_property, true);
    if (i) cursor.insertBlock(block, QTextCharFormat());
    else cursor.setBlockFormat(block);
    std::vector<std::size_t> bounds{0, element.text.size()};
    for (const auto& span : element.styles) { bounds.push_back(span.start); bounds.push_back(span.end); }
    std::sort(bounds.begin(), bounds.end());
    bounds.erase(std::unique(bounds.begin(), bounds.end()), bounds.end());
    for (std::size_t run = 1; run < bounds.size(); ++run) {
      QTextCharFormat format;
      for (const auto& span : element.styles) if (span.start <= bounds[run - 1] && span.end >= bounds[run]) {
        if (span.style == Style::italic) format.setFontItalic(true);
        if (span.style == Style::bold) format.setFontWeight(QFont::Bold);
        if (span.style == Style::underline) format.setFontUnderline(true);
      }
      cursor.insertText(QString::fromUtf8(element.text.data() + bounds[run - 1],
                                          static_cast<qsizetype>(bounds[run] - bounds[run - 1])), format);
    }
  }
  document->clearUndoRedoStacks();
  document->setModified(false);
  // Installing the layout last runs the engine once on the loaded text.
  document->setDocumentLayout(new ScreenplayLayout(document.get()));
  return document;
}

std::vector<Element> document_elements(const QTextDocument& document) {
  std::vector<Element> elements;
  for (auto block = document.begin(); block != document.end(); block = block.next()) {
    const auto format = block.blockFormat();
    const auto type = format.hasProperty(element_type_property)
                          ? static_cast<ElementType>(format.intProperty(element_type_property)) : ElementType::action;
    Element element{type, block.text().toUtf8().toStdString(), format.boolProperty(dual_right_property)};
    std::size_t byte = 0;
    for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
      const auto& part = fragment.fragment();
      const auto size = static_cast<std::size_t>(part.text().toUtf8().size());
      const auto format = part.charFormat();
      if (format.fontItalic()) element.styles.push_back({byte, byte + size, Style::italic});
      if (format.fontWeight() >= QFont::Bold) element.styles.push_back({byte, byte + size, Style::bold});
      if (format.fontUnderline()) element.styles.push_back({byte, byte + size, Style::underline});
      byte += size;
    }
    elements.push_back(std::move(element));
  }
  return elements;
}

ScreenplayLayout::ScreenplayLayout(QTextDocument* document) : QAbstractTextDocumentLayout(document) {}

QFont ScreenplayLayout::font() const { return document()->defaultFont(); }

QSizeF ScreenplayLayout::documentSize() const { return {page_width, pages_ * page_stride - page_gap}; }

QRectF ScreenplayLayout::frameBoundingRect(QTextFrame*) const { return {{0, 0}, documentSize()}; }

QRectF ScreenplayLayout::blockBoundingRect(const QTextBlock& block) const {
  const auto number = static_cast<std::size_t>(block.blockNumber());
  return block.isValid() && number < block_rects_.size() ? block_rects_[number] : QRectF();
}

QVariantList ScreenplayLayout::decorations() const {
  QVariantList list;
  for (const auto& decoration : decorations_)
    list.push_back(QVariantMap{{"text", decoration.text}, {"x", decoration.baseline.x()}, {"baseline", decoration.baseline.y()}});
  return list;
}

// Stock TextEdit rebuilds every text node when the text changes, so moved
// blocks need no separate notification. Size and decorations do.
void ScreenplayLayout::documentChanged(int, int, int) {
  relayout();
  emit documentSizeChanged(documentSize());
  emit pageCountChanged(pages_);
  emit update();
  emit layoutChanged();
}

void ScreenplayLayout::relayout() {
  const auto elements = document_elements(*document());
  DocumentLayout laid;
  try {
    laid = layout_document(elements);
    error_.clear();
  } catch (const std::exception& failure) {
    error_ = QString::fromUtf8(failure.what());
    fallback_layout();
    return;
  }
  std::vector<std::vector<const Line*>> by_block(elements.size());
  decorations_.clear();
  pages_ = 1;
  for (const auto& page : laid.pages) {
    pages_ = std::max(pages_, page.body_number);
    if (page.number && page.body_number > 0)
      decorations_.push_back({QString::fromUtf8(page.number->text),
                              {snap(page.number->geometry.x * px_per_point),
                               (page.body_number - 1) * page_stride + page.number->geometry.baseline * px_per_point}});
  }
  for (const auto& line : laid.lines) {
    if (line.page == 0) continue; // The probe does not edit the title page.
    const QPointF baseline(snap(line.geometry.x * px_per_point),
                           (line.page - 1) * page_stride + line.geometry.baseline * px_per_point);
    if (line.source == no_source) { decorations_.push_back({QString::fromUtf8(line.text), baseline}); continue; }
    by_block[line.source].push_back(&line);
    if (line.source_length < line.text.size()) {
      const auto prefix = QString::fromUtf8(line.text.data(), static_cast<qsizetype>(line.source_length));
      const auto x = (line.geometry.x + prefix.toUcs4().size() * profile::advance) * px_per_point;
      decorations_.push_back({QString::fromUtf8(line.text.substr(line.source_length)), {snap(x), baseline.y()}});
    }
  }
  block_rects_.assign(elements.size(), QRectF());
  double previous_baseline = 81 * px_per_point - 2 * pitch;
  auto block = document()->begin();
  for (std::size_t index = 0; index < elements.size(); ++index, block = block.next()) {
    auto& lines = by_block[index];
    std::stable_sort(lines.begin(), lines.end(), [](auto a, auto b) { return a->source_offset < b->source_offset; });
    const auto& bytes = elements[index].text;
    const int length = static_cast<int>(block.text().size());
    auto* text_layout = block.layout();
    // Input-method preedit text is part of the layout's text, not the block's.
    // It stays on the line where it is inserted until it is committed.
    const int preedit_at = text_layout->preeditAreaPosition();
    const auto preedit = static_cast<int>(text_layout->preeditAreaText().size());
    auto layout_index = [&](int index) { return preedit && index > preedit_at ? index + preedit : index; };
    text_layout->setTextOption(line_option());
    text_layout->beginLayout();
    QPointF origin(0, 0);
    qreal bottom = 0;
    // Line positions are relative to the block origin, the first line's top.
    auto place = [&](QTextLine& line, double x, double baseline, int first_visible) {
      if (line.lineNumber() == 0) origin.setY(baseline - line.ascent());
      const auto lead = line.cursorToX(first_visible) - line.cursorToX(line.textStart());
      line.setPosition({snap(x - lead), baseline - line.ascent() - origin.y()});
      bottom = std::max(bottom, origin.y() + line.y() + line.height());
      previous_baseline = baseline;
    };
    if (lines.empty()) {
      // The engine gives empty or title elements no body line, but the caret
      // still needs one. This placement is a probe stand-in, not a profile rule.
      auto line = text_layout->createLine();
      line.setNumColumns(length + preedit);
      place(line, left_margin, previous_baseline + 2 * pitch, 0);
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
      const int start = layout_index(utf16_index(bytes, lines[i]->source_offset));
      const int end = i + 1 < lines.size() ? layout_index(utf16_index(bytes, lines[i + 1]->source_offset)) : length + preedit;
      auto line = text_layout->createLine();
      line.setNumColumns(end - line.textStart());
      if (line.textStart() + line.textLength() != end && error_.isEmpty())
        error_ = QStringLiteral("QTextLayout did not break block %1 where the engine did").arg(index);
      place(line, lines[i]->geometry.x * px_per_point,
            (lines[i]->page - 1) * page_stride + lines[i]->geometry.baseline * px_per_point, start);
    }
    text_layout->endLayout();
    text_layout->setPosition(origin);
    block_rects_[index] = QRectF(0, origin.y(), page_width, bottom - origin.y());
  }
}

// Keeps every block editable while the engine rejects an in-progress document,
// for example a cue that has no dialogue yet. The result is not paginated.
void ScreenplayLayout::fallback_layout() {
  decorations_.clear();
  block_rects_.clear();
  qreal y = 72 * px_per_point;
  for (auto block = document()->begin(); block != document()->end(); block = block.next()) {
    auto* text_layout = block.layout();
    text_layout->setTextOption(line_option());
    text_layout->beginLayout();
    const qreal top = y;
    for (auto line = text_layout->createLine(); line.isValid(); line = text_layout->createLine()) {
      line.setLineWidth(61 * profile::advance * px_per_point);
      line.setPosition({left_margin, y - top});
      y += pitch;
    }
    text_layout->endLayout();
    text_layout->setPosition({0, top});
    block_rects_.push_back(QRectF(0, top, page_width, y - top));
    y += pitch;
  }
  pages_ = std::max(1, static_cast<int>(std::ceil(y / page_stride)));
}

int ScreenplayLayout::hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const {
  // Each line owns one slot-high band from its top, so bands never overlap.
  int best = -1;
  qreal best_dy = std::numeric_limits<qreal>::max(), best_dx = best_dy;
  for (auto block = document()->begin(); block != document()->end(); block = block.next()) {
    const auto* text_layout = block.layout();
    const auto origin = blockBoundingRect(block).topLeft();
    for (int i = 0; text_layout && i < text_layout->lineCount(); ++i) {
      const auto line = text_layout->lineAt(i);
      const QRectF band(origin.x() + line.x(), origin.y() + line.y(), line.naturalTextWidth(), pitch);
      const qreal dy = std::max({band.top() - point.y(), point.y() - band.bottom(), qreal(0)});
      const qreal dx = std::max({band.left() - point.x(), point.x() - band.right(), qreal(0)});
      if (accuracy == Qt::ExactHit && (dx > 0 || dy > 0)) continue;
      if (dy < best_dy || (dy == best_dy && dx < best_dx)) {
        best_dy = dy; best_dx = dx;
        best = block.position() + line.xToCursor(point.x() - origin.x());
      }
    }
  }
  return best;
}

void ScreenplayLayout::draw(QPainter* painter, const PaintContext& context) {
  for (auto block = document()->begin(); block != document()->end(); block = block.next()) {
    QList<QTextLayout::FormatRange> selections;
    const int start = block.position(), end = start + block.length();
    for (const auto& selection : context.selections) {
      const int from = std::max(selection.cursor.selectionStart(), start), to = std::min(selection.cursor.selectionEnd(), end);
      if (from < to) selections.push_back({from - start, to - from, selection.format});
    }
    const auto origin = blockBoundingRect(block).topLeft();
    block.layout()->draw(painter, origin, selections, context.clip);
    if (context.cursorPosition >= start && context.cursorPosition < end)
      block.layout()->drawCursor(painter, origin, context.cursorPosition - start);
  }
  painter->setFont(font());
  painter->setPen(context.palette.color(QPalette::Text));
  for (const auto& decoration : decorations_) painter->drawText(decoration.baseline, decoration.text);
}
}
