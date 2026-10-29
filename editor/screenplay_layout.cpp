#include "editor/screenplay_layout.h"
#include "source/screenplay_format.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QPainter>
#include <QScopedValueRollback>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

namespace apollo::editor {
namespace {
constexpr double left_margin = 108 * px_per_point, pitch = profile::line_pitch * px_per_point;
// Marks the layout formats added here, so an input method's formats survive.
constexpr int own_format_property = QTextFormat::UserProperty + 3;
constexpr int page_slots = 56;

// QTextLine stores positions in 1/64 px; round rather than let it truncate.
double snap(double px) { return std::round(px * 64) / 64; }

QTextOption line_option() {
  // Lines are broken only where the engine says, including inside a long word.
  QTextOption option(Qt::AlignLeft);
  option.setWrapMode(QTextOption::WrapAnywhere);
  option.setUseDesignMetrics(true);
  return option;
}

// UTF-16 index of a UTF-8 byte offset into one block's text.
struct Utf16Index {
  const std::string& text;
  bool ascii = std::all_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
  int operator()(std::size_t byte) const {
    return ascii ? static_cast<int>(byte) : static_cast<int>(QString::fromUtf8(text.data(), static_cast<qsizetype>(byte)).size());
  }
};

bool uppercase_type(ElementType type) {
  return type == ElementType::cue || type == ElementType::heading || type == ElementType::title;
}

QString kind_name(ElementType type, bool continuation) {
  switch (type) {
    case ElementType::heading: return QStringLiteral("heading");
    case ElementType::cue: return QStringLiteral("cue");
    case ElementType::parenthetical: return continuation ? QStringLiteral("paren+") : QStringLiteral("paren");
    case ElementType::dialogue: return QStringLiteral("dialogue");
    case ElementType::more: return QStringLiteral("more");
    case ElementType::lyric: return QStringLiteral("lyric");
    case ElementType::transition: return QStringLiteral("transition");
    case ElementType::centered: return QStringLiteral("centered");
    case ElementType::title: return QStringLiteral("title@center");
    case ElementType::credit: return QStringLiteral("credit@center");
    case ElementType::author: return QStringLiteral("author@center");
    case ElementType::source: return QStringLiteral("source@center");
    case ElementType::contact: return QStringLiteral("contact@5.2in");
    case ElementType::draft_date: return QStringLiteral("draft-date@1.5in");
    case ElementType::page_break: return QStringLiteral("page-break");
    case ElementType::action: break;
  }
  return QStringLiteral("action");
}

QString line_kind(const Line& line) {
  const auto kind = kind_name(line.type, line.continuation);
  return line.column ? QStringLiteral("dual-%1-%2").arg(kind).arg(QChar(line.column)) : kind;
}

// One line of a block as the engine places it, in layout text indexes.
struct Placement {
  int start, end, first_visible;
  double x, baseline;
  bool operator==(const Placement&) const = default;
};

// What a block was last laid out with; if nothing changed, only its
// position needs updating.
struct BlockCache : QTextBlockUserData {
  std::vector<Placement> placements;  // baselines relative to the first line
  QList<QTextLayout::FormatRange> own;
};

std::vector<Placement> relative(std::vector<Placement> placements) {
  const double first = placements.empty() ? 0 : placements.front().baseline;
  for (auto& placement : placements) placement.baseline -= first;
  return placements;
}

std::vector<StyleSpan> shifted_styles(const std::vector<StyleSpan>& styles, std::size_t from) {
  std::vector<StyleSpan> out;
  for (auto span : styles) {
    if (span.end <= from) continue;
    span.start = span.start > from ? span.start - from : 0;
    span.end -= from;
    out.push_back(span);
  }
  return out;
}

int columns(const std::string& bytes, std::size_t end) {
  return static_cast<int>(std::count_if(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(end),
                                        [](char c) { return (static_cast<unsigned char>(c) & 0xc0) != 0x80; }));
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

void insert_runs(QTextCursor& cursor, const Element& element) {
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
    auto text = QString::fromUtf8(element.text.data() + bounds[run - 1], static_cast<qsizetype>(bounds[run] - bounds[run - 1]));
    cursor.insertText(text.replace(QLatin1Char('\n'), QChar::LineSeparator), format);
  }
}

void insert_elements(QTextCursor& cursor, const std::vector<Element>& elements) {
  for (std::size_t i = 0; i < elements.size(); ++i) {
    QTextBlockFormat block;
    block.setProperty(element_type_property, static_cast<int>(elements[i].type));
    if (elements[i].dual_right) block.setProperty(dual_right_property, true);
    block.setProperty(element_id_property,
                      QString::fromStdString(elements[i].id.empty() ? source::new_element_id() : elements[i].id));
    if (i) cursor.insertBlock(block, QTextCharFormat());
    else cursor.setBlockFormat(block);
    insert_runs(cursor, elements[i]);
  }
}

std::pair<Element, Element> split_element(const Element& element, std::size_t byte) {
  Element head{element.type, element.text.substr(0, byte), element.dual_right};
  Element tail{element.type, element.text.substr(byte), element.dual_right};
  for (const auto& span : element.styles) {
    if (span.start < byte) head.styles.push_back({span.start, std::min(span.end, byte), span.style});
    if (span.end > byte) tail.styles.push_back({span.start > byte ? span.start - byte : 0, span.end - byte, span.style});
  }
  (head.text.empty() && !tail.text.empty() ? tail : head).id = element.id;
  return {head, tail};
}

std::unique_ptr<QTextDocument> screenplay_document(const std::vector<Element>& elements, const QFont& font) {
  auto document = std::make_unique<QTextDocument>();
  document->setDefaultFont(font);
  QTextCursor cursor(document.get());
  insert_elements(cursor, elements);
  repair_identities(*document);
  document->clearUndoRedoStacks();
  document->setModified(false);
  // Installing the layout last runs the engine once on the loaded text.
  document->setDocumentLayout(new ScreenplayLayout(document.get()));
  // Repair Qt-created blocks synchronously when their undo command is
  // added, before another edit can occur. Joining that command preserves
  // the same identity on redo. Saves and undo/redo never mutate identities.
  QObject::connect(document.get(), &QTextDocument::undoCommandAdded, document.get(),
                   [document = document.get(), repairing = false]() mutable {
    if (repairing) return;
    QScopedValueRollback guard(repairing, true);
    repair_identities(*document);
  });
  return document;
}

ElementType element_type(const QTextBlock& block) {
  const auto format = block.blockFormat();
  return format.hasProperty(element_type_property) ? static_cast<ElementType>(format.intProperty(element_type_property))
                                                   : ElementType::action;
}

std::string element_text(QString text) {
  return text.replace(QChar::LineSeparator, QLatin1Char('\n')).toUtf8().toStdString();
}

Element block_element(const QTextBlock& block) {
  const auto properties = block.blockFormat();
  Element element{element_type(block), element_text(block.text()), properties.boolProperty(dual_right_property), {},
                  properties.stringProperty(element_id_property).toStdString()};
  std::size_t byte = 0;
  for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
    const auto& part = fragment.fragment();
    const auto size = element_text(part.text()).size();
    const auto format = part.charFormat();
    if (format.fontItalic()) element.styles.push_back({byte, byte + size, Style::italic});
    if (format.fontWeight() >= QFont::Bold) element.styles.push_back({byte, byte + size, Style::bold});
    if (format.fontUnderline()) element.styles.push_back({byte, byte + size, Style::underline});
    byte += size;
  }
  return element;
}

int repair_identities(QTextDocument& document) {
  std::set<QString> seen;
  int repaired = 0;
  QTextCursor cursor(&document);
  for (auto block = document.begin(); block != document.end(); block = block.next()) {
    const auto id = block.blockFormat().stringProperty(element_id_property);
    if (!id.isEmpty() && seen.insert(id).second) continue;
    if (!repaired++) cursor.joinPreviousEditBlock();
    cursor.setPosition(block.position());
    QTextBlockFormat format;
    format.setProperty(element_id_property, QString::fromStdString(source::new_element_id()));
    cursor.mergeBlockFormat(format);
    seen.insert(format.stringProperty(element_id_property));
  }
  if (repaired) cursor.endEditBlock();
  return repaired;
}

std::vector<Element> document_elements(const QTextDocument& document) {
  std::vector<Element> elements;
  elements.reserve(static_cast<std::size_t>(document.blockCount()));
  for (auto block = document.begin(); block != document.end(); block = block.next()) elements.push_back(block_element(block));
  return elements;
}

int DecorationModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(runs_.size());
}

QVariant DecorationModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() >= rowCount()) return {};
  const auto& run = runs_[static_cast<std::size_t>(index.row())];
  switch (role) {
    case TextRole: return run.text;
    case XRole: return run.x;
    case BaselineRole: return run.baseline;
    case BoldRole: return run.bold;
    case ItalicRole: return run.italic;
    case UnderlineRole: return run.underline;
    default: return {};
  }
}

QHash<int, QByteArray> DecorationModel::roleNames() const {
  return {{TextRole, "label"}, {XRole, "runX"}, {BaselineRole, "runBaseline"},
          {BoldRole, "bold"}, {ItalicRole, "italic"}, {UnderlineRole, "underline"}};
}

void DecorationModel::replace(std::vector<Run> runs) {
  // Keep the unchanged prefix and suffix; replace only the rows between.
  std::size_t prefix = 0;
  while (prefix < runs.size() && prefix < runs_.size() && runs[prefix] == runs_[prefix]) ++prefix;
  std::size_t suffix = 0;
  while (suffix < runs.size() - prefix && suffix < runs_.size() - prefix &&
         runs[runs.size() - 1 - suffix] == runs_[runs_.size() - 1 - suffix]) ++suffix;
  const auto old_middle = runs_.size() - prefix - suffix, new_middle = runs.size() - prefix - suffix;
  const int first = static_cast<int>(prefix);
  if (old_middle == new_middle) {
    runs_ = std::move(runs);
    if (new_middle) emit dataChanged(index(first), index(first + static_cast<int>(new_middle) - 1));
    return;
  }
  if (old_middle) {
    beginRemoveRows({}, first, first + static_cast<int>(old_middle) - 1);
    runs_.erase(runs_.begin() + first, runs_.begin() + first + static_cast<std::ptrdiff_t>(old_middle));
    endRemoveRows();
  }
  if (new_middle) {
    beginInsertRows({}, first, first + static_cast<int>(new_middle) - 1);
    runs_.insert(runs_.begin() + first, runs.begin() + first, runs.begin() + first + static_cast<std::ptrdiff_t>(new_middle));
    endInsertRows();
  }
}

ScreenplayLayout::ScreenplayLayout(QTextDocument* document) : QAbstractTextDocumentLayout(document), model_(this) {}

QFont ScreenplayLayout::font() const { return document()->defaultFont(); }

QSizeF ScreenplayLayout::documentSize() const { return {page_width, pages_ * page_stride - page_gap}; }

QRectF ScreenplayLayout::frameBoundingRect(QTextFrame*) const { return {{0, 0}, documentSize()}; }

QRectF ScreenplayLayout::blockBoundingRect(const QTextBlock& block) const {
  const auto number = static_cast<std::size_t>(block.blockNumber());
  return block.isValid() && number < block_rects_.size() ? block_rects_[number] : QRectF();
}

// TextEdit marks its text nodes dirty when the text changes, so moved
// blocks need no separate notification. Size and decorations do.
void ScreenplayLayout::documentChanged(int, int, int) {
  QElapsedTimer timer; timer.start();
  relayout();
  relayout_nanoseconds_ += timer.nsecsElapsed();
  emit documentSizeChanged(documentSize());
  emit pageCountChanged(pages_);
  emit update();
  emit layoutChanged();
}

void ScreenplayLayout::relayout() {
  // TextEdit traverses rootFrame() during render-thread synchronization.
  // Qt creates it lazily; prepare it on the document's GUI thread, including
  // after clear() discards the old root, before rendering can access it.
  document()->rootFrame();
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
  title_page_ = std::any_of(laid.lines.begin(), laid.lines.end(), [](const Line& line) { return line.page == 0; });
  auto baseline_y = [&](int page, double baseline) { return page_top(page) + baseline * px_per_point; };
  std::vector<std::vector<const Line*>> by_block(elements.size());
  decorations_.clear();
  int body_pages = 1;
  for (const auto& page : laid.pages) {
    body_pages = std::max(body_pages, page.body_number);
    if (page.number && page.body_number > 0)
      decorations_.push_back({QString::fromUtf8(page.number->text),
                              {snap(page.number->geometry.x * px_per_point), baseline_y(page.body_number, page.number->geometry.baseline)},
                              QStringLiteral("number"), {}});
  }
  pages_ = body_pages + (title_page_ ? 1 : 0);
  for (const auto& line : laid.lines) {
    const QPointF baseline(snap(line.geometry.x * px_per_point), baseline_y(line.page, line.geometry.baseline));
    if (line.source == no_source) {
      decorations_.push_back({QString::fromUtf8(line.text), baseline, line_kind(line), line.styles});
      continue;
    }
    by_block[line.source].push_back(&line);
    if (line.source_length < line.text.size()) {
      const auto x = (line.geometry.x + columns(line.text, line.source_length) * profile::advance) * px_per_point;
      decorations_.push_back({QString::fromUtf8(line.text.substr(line.source_length)), {snap(x), baseline.y()},
                              line_kind(line), shifted_styles(line.styles, line.source_length)});
    }
  }
  std::vector<DecorationModel::Run> runs;
  for (const auto& decoration : decorations_) {
    const auto bytes = decoration.text.toUtf8().toStdString();
    std::vector<std::size_t> bounds{0, bytes.size()};
    for (const auto& span : decoration.styles) { bounds.push_back(span.start); bounds.push_back(std::min(span.end, bytes.size())); }
    std::sort(bounds.begin(), bounds.end());
    bounds.erase(std::unique(bounds.begin(), bounds.end()), bounds.end());
    for (std::size_t i = 1; i < bounds.size(); ++i) {
      DecorationModel::Run run{QString::fromUtf8(bytes.data() + bounds[i - 1], static_cast<qsizetype>(bounds[i] - bounds[i - 1])),
                               snap(decoration.baseline.x() + columns(bytes, bounds[i - 1]) * profile::advance * px_per_point),
                               decoration.baseline.y(), false, false, false};
      for (const auto& span : decoration.styles) if (span.start <= bounds[i - 1] && span.end >= bounds[i]) {
        run.bold |= span.style == Style::bold;
        run.italic |= span.style == Style::italic;
        run.underline |= span.style == Style::underline;
      }
      runs.push_back(std::move(run));
    }
  }
  model_.replace(std::move(runs));

  block_rects_.assign(elements.size(), QRectF());
  rebuilt_blocks_ = 0;
  // A manual break has no engine line. Its caret line goes after the last
  // line of the page it ends, or into the gap below a full page.
  int current_page = 1, current_slot = 0;
  auto block = document()->begin();
  for (std::size_t index = 0; index < elements.size(); ++index, block = block.next()) {
    auto& lines = by_block[index];
    std::stable_sort(lines.begin(), lines.end(), [](auto a, auto b) { return a->source_offset < b->source_offset; });
    const Utf16Index utf16{elements[index].text};
    const int length = static_cast<int>(block.text().size());
    auto* text_layout = block.layout();
    // Input-method preedit text is part of the layout's text, not the block's.
    // It stays on the line where it is inserted until it is committed.
    const int preedit_at = text_layout->preeditAreaPosition();
    const auto preedit = static_cast<int>(text_layout->preeditAreaText().size());
    auto layout_index = [&](int at) { return preedit && at > preedit_at ? at + preedit : at; };
    std::vector<Placement> placements;
    QList<QTextLayout::FormatRange> own;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      const auto& line = *lines[i];
      const int end = i + 1 < lines.size() ? layout_index(utf16(lines[i + 1]->source_offset)) : length + preedit;
      placements.push_back({i ? placements.back().end : 0, end, layout_index(utf16(line.source_offset)),
                            snap(line.geometry.x * px_per_point), baseline_y(line.page, line.geometry.baseline)});
      if (line.replaced_length) {
        // A typed suffix the engine replaced is drawn under the generated one.
        QTextCharFormat hidden;
        hidden.setForeground(Qt::transparent);
        hidden.setProperty(own_format_property, true);
        const int from = layout_index(utf16(line.source_offset + line.source_length));
        own.push_back({from, layout_index(utf16(line.source_offset + line.source_length + line.replaced_length)) - from, hidden});
      }
      if (line.page > current_page) current_slot = 0;
      if (line.page >= current_page) { current_page = line.page; current_slot = std::max(current_slot, line.slot); }
    }
    if (lines.empty()) {
      // Only a manual break gets here: every other element has an engine line.
      const double baseline = current_slot >= page_slots
          ? page_top(current_page) + page_height + page_gap * 0.75
          : baseline_y(current_page, 81 + std::min(current_slot + (current_slot ? 1 : 0), page_slots - 1) * profile::line_pitch);
      placements.push_back({0, length + preedit, 0, left_margin, baseline});
      if (elements[index].type == ElementType::page_break) { ++current_page; current_slot = 0; }
    }
    if (uppercase_type(elements[index].type)) {
      // Shown uppercase like the engine's text: ASCII letters only.
      const auto text = block.text();
      QTextCharFormat caps;
      caps.setFontCapitalization(QFont::AllUppercase);
      caps.setProperty(own_format_property, true);
      for (int at = 0; at < length;) {
        if (text[at] < QLatin1Char('a') || text[at] > QLatin1Char('z')) { ++at; continue; }
        int to = at;
        while (to < length && text[to] >= QLatin1Char('a') && text[to] <= QLatin1Char('z')) ++to;
        own.push_back({layout_index(at), layout_index(to) - layout_index(at), caps});
        at = to;
      }
    }

    auto* cache = static_cast<BlockCache*>(block.userData());
    const auto shape = relative(placements);
    const bool reuse = cache && text_layout->lineCount() == static_cast<int>(placements.size()) &&
                       cache->placements == shape && cache->own == own;
    if (!reuse) {
      ++rebuilt_blocks_;
      auto formats = text_layout->formats();
      formats.removeIf([](const QTextLayout::FormatRange& range) { return range.format.hasProperty(own_format_property); });
      formats.append(own);
      if (formats != text_layout->formats()) text_layout->setFormats(formats);
      text_layout->setTextOption(line_option());
      text_layout->beginLayout();
      double origin_y = 0;
      for (const auto& placement : placements) {
        auto line = text_layout->createLine();
        line.setNumColumns(placement.end - line.textStart());
        if (line.textStart() + line.textLength() != placement.end && error_.isEmpty())
          error_ = QStringLiteral("QTextLayout did not break block %1 where the engine did").arg(index);
        // Line positions are relative to the block origin, the first line's top.
        if (line.lineNumber() == 0) origin_y = placement.baseline - line.ascent();
        const auto lead = line.cursorToX(placement.first_visible) - line.cursorToX(line.textStart());
        line.setPosition({snap(placement.x - lead), placement.baseline - line.ascent() - origin_y});
      }
      text_layout->endLayout();
      if (!cache) { cache = new BlockCache; block.setUserData(cache); }
      cache->placements = shape;
      cache->own = own;
    }
    const double origin_y = placements.front().baseline - text_layout->lineAt(0).ascent();
    text_layout->setPosition({0, origin_y});
    double bottom = origin_y;
    for (int i = 0; i < text_layout->lineCount(); ++i) {
      const auto line = text_layout->lineAt(i);
      bottom = std::max(bottom, origin_y + line.y() + line.height());
    }
    block_rects_[index] = QRectF(0, origin_y, page_width, bottom - origin_y);
  }
}

// Keeps every block editable while the engine rejects the document, for
// example an element longer than a page. The result is not paginated.
void ScreenplayLayout::fallback_layout() {
  decorations_.clear();
  model_.replace({});
  block_rects_.clear();
  title_page_ = false;
  qreal y = 72 * px_per_point;
  for (auto block = document()->begin(); block != document()->end(); block = block.next()) {
    block.setUserData(nullptr);
    auto* text_layout = block.layout();
    // Without decorations, text hidden under a generated suffix must show.
    auto formats = text_layout->formats();
    if (formats.removeIf([](const QTextLayout::FormatRange& range) { return range.format.hasProperty(own_format_property); }))
      text_layout->setFormats(formats);
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

std::vector<ShownLine> shown_lines(const QTextDocument& document) {
  const auto* layout = static_cast<const ScreenplayLayout*>(document.documentLayout());
  auto locate = [&](double baseline_px, ShownLine& shown) {
    const int card = static_cast<int>(std::floor(baseline_px / page_stride));
    shown.page = card + (layout->has_title_page() ? 0 : 1);
    const double rows = ((baseline_px - card * page_stride) / px_per_point - 81) / profile::line_pitch;
    shown.slot = std::abs(rows - std::round(rows)) < 1e-3 ? static_cast<int>(std::round(rows)) + 1 : -1;
  };
  std::vector<ShownLine> shown, numbers;
  for (auto block = document.begin(); block != document.end(); block = block.next()) {
    const auto type = element_type(block);
    const auto* text_layout = block.layout();
    const auto origin = layout->blockBoundingRect(block).topLeft();
    const auto block_text = block.text();
    std::vector<bool> hidden(static_cast<std::size_t>(block_text.size()));
    for (const auto& range : text_layout->formats())
      if (range.format.hasProperty(own_format_property) && range.format.foreground().color().alpha() == 0)
        for (int i = range.start; i < range.start + range.length && i < block_text.size(); ++i) hidden[static_cast<std::size_t>(i)] = true;
    for (int i = 0; i < text_layout->lineCount(); ++i) {
      const auto line = text_layout->lineAt(i);
      QString text;
      for (int at = line.textStart(); at < line.textStart() + line.textLength() && at < block_text.size(); ++at)
        if (!hidden[static_cast<std::size_t>(at)] && block_text[at] != QChar::LineSeparator) text += block_text[at];
      qsizetype lead = 0;
      while (lead < text.size() && text[lead] == QLatin1Char(' ')) ++lead;
      text = text.mid(lead);
      while (text.endsWith(QLatin1Char(' '))) text.chop(1);
      if (text.isEmpty()) continue;
      if (uppercase_type(type))
        for (auto& c : text) if (c >= QLatin1Char('a') && c <= QLatin1Char('z')) c = c.toUpper();
      ShownLine piece{0, 0, kind_name(type, type == ElementType::parenthetical && i > 0),
                      (origin.x() + line.cursorToX(line.textStart() + static_cast<int>(lead))) / px_per_point, text};
      locate(origin.y() + line.y() + line.ascent(), piece);
      // Dual speech is identified by its columns (profile: left 1.5 in, right 4.6 in).
      if (type == ElementType::cue || type == ElementType::dialogue || type == ElementType::parenthetical) {
        const double normal = type == ElementType::cue ? 288 : type == ElementType::dialogue ? 216 : (i ? 252 : 244.8);
        if (std::abs(piece.x - normal) > 0.01)
          piece.kind = QStringLiteral("dual-%1-%2").arg(piece.kind).arg(piece.x < normal ? 'L' : 'R');
      }
      shown.push_back(piece);
    }
  }
  for (const auto& decoration : layout->decoration_list()) {
    ShownLine piece{0, 0, decoration.kind, decoration.baseline.x() / px_per_point, decoration.text};
    locate(decoration.baseline.y(), piece);
    if (decoration.kind == QStringLiteral("number")) { piece.slot = 0; numbers.push_back(piece); continue; }
    // A generated suffix continues the line it ends.
    auto joined = std::find_if(shown.begin(), shown.end(), [&](const ShownLine& line) {
      return line.page == piece.page && line.slot == piece.slot &&
             std::abs(line.x + static_cast<double>(line.text.toUcs4().size()) * profile::advance - piece.x) < 0.01;
    });
    if (joined != shown.end()) joined->text += piece.text;
    else shown.push_back(piece);
  }
  shown.insert(shown.end(), numbers.begin(), numbers.end());
  std::sort(shown.begin(), shown.end(), [](const ShownLine& a, const ShownLine& b) {
    return std::tie(a.page, a.slot, a.x) < std::tie(b.page, b.slot, b.x);
  });
  return shown;
}
}
