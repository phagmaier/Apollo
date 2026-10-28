#include "export/pdf.h"

#include <QDir>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QRawFont>
#include <QSaveFile>
#include <QScopeGuard>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace apollo {
namespace {
using Fonts = std::array<QRawFont, 4>;

// Each span boundary is a UTF-8 byte boundary supplied by the engine. Painting
// uses explicit glyph origins so Qt never rewraps or changes the fixed advance.
void paint_text(QPainter* painter, const std::string& text, const TextGeometry& geometry,
                const std::vector<StyleSpan>& styles, const Fonts& fonts) {
  std::vector<std::size_t> boundaries{0, text.size()};
  for (const auto& span : styles) {
    if (span.start > span.end || span.end > text.size()) throw std::runtime_error("invalid style span");
    boundaries.push_back(span.start); boundaries.push_back(span.end);
  }
  std::sort(boundaries.begin(), boundaries.end());
  boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
  int column = 0;
  for (std::size_t part = 1; part < boundaries.size(); ++part) {
    const auto start = boundaries[part - 1], end = boundaries[part];
    const auto bytes = text.substr(start, end - start);
    const auto source = QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size()));
    if (source.toUtf8().toStdString() != bytes) throw std::runtime_error("invalid UTF-8 or style boundary");
    int face = 0;
    bool underline = false;
    for (const auto& style : styles) if (style.start <= start && style.end >= end) {
      if (style.style == Style::italic) face |= 1;
      if (style.style == Style::bold) face |= 2;
      if (style.style == Style::underline) underline = true;
    }
    const auto& font = fonts[face];
    const auto glyphs = font.glyphIndexesForString(source);
    if (glyphs.size() != source.size() || std::any_of(glyphs.begin(), glyphs.end(), [](auto glyph) { return glyph == 0; }))
      throw std::runtime_error("text contains a character unsupported by bundled Courier Prime");
    QList<QPointF> positions;
    QList<qsizetype> indexes;
    for (qsizetype index = 0; index < glyphs.size(); ++index) {
      positions.push_back({(column + index) * profile::advance, 0});
      indexes.push_back(index);
    }
    if (painter) {
      QGlyphRun run;
      run.setRawFont(font); run.setGlyphIndexes(glyphs); run.setPositions(positions);
      run.setSourceString(source); run.setStringIndexes(indexes);
      painter->drawGlyphRun(QPointF(geometry.x, geometry.baseline), run);
      if (underline) {
        const auto post = font.fontTable("post");
        if (post.size() < 12) throw std::runtime_error("font has no underline metrics");
        const double scale = profile::font_size / font.unitsPerEm();
        const double y = geometry.baseline - qFromBigEndian<qint16>(post.constData() + 8) * scale;
        QPen pen(Qt::black);
        pen.setWidthF(qFromBigEndian<qint16>(post.constData() + 10) * scale);
        painter->setPen(pen);
        painter->drawLine(QPointF(geometry.x + column * profile::advance, y),
                          QPointF(geometry.x + (column + glyphs.size()) * profile::advance, y));
        painter->setPen(Qt::black);
      }
    }
    column += static_cast<int>(glyphs.size());
  }
}
}

void export_pdf(const DocumentLayout& document, const QString& destination,
                const QString& font_directory, bool include_title) {
  Fonts fonts;
  QList<int> registrations;
  const auto unregister = qScopeGuard([&] { for (int id : registrations) QFontDatabase::removeApplicationFont(id); });
  const std::array<const char*, 4> names{"Regular", "Italic", "Bold", "BoldItalic"};
  for (std::size_t index = 0; index < names.size(); ++index) {
    const auto path = QDir(font_directory).filePath(QString("CourierPrime-%1.ttf").arg(names[index]));
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0) throw std::runtime_error("cannot register bundled Courier Prime font");
    registrations.push_back(id);
  }
  for (std::size_t index = 0; index < names.size(); ++index) {
    QFont font(QFontDatabase::applicationFontFamilies(registrations[index]).front());
    font.setPixelSize(static_cast<int>(profile::font_size));
    font.setBold(index & 2); font.setItalic(index & 1);
    font.setKerning(false); font.setHintingPreference(QFont::PreferNoHinting);
    font.setStyleStrategy(QFont::NoFontMerging);
    fonts[index] = QRawFont::fromFont(font);
    if (!fonts[index].isValid()) throw std::runtime_error("cannot load bundled Courier Prime font");
  }
  // Reject unsupported text before writing, without silent font substitution.
  // The product's broader unexpected-character policy is still undecided.
  for (const auto& line : document.lines) if (include_title || line.page != 0)
    paint_text(nullptr, line.text, line.geometry, line.styles, fonts);
  QSaveFile output(destination);
  if (!output.open(QIODevice::WriteOnly)) throw std::runtime_error(output.errorString().toStdString());
  {
    QPdfWriter writer(&output);
    writer.setResolution(72);
    writer.setCreator("Apollo feasibility spike");
    QPageLayout page_layout(QPageSize(QPageSize::Letter), QPageLayout::Portrait, QMarginsF(), QPageLayout::Point);
    page_layout.setMode(QPageLayout::FullPageMode);
    if (!writer.setPageLayout(page_layout)) throw std::runtime_error("cannot configure PDF page");
    QPainter painter;
    if (!painter.begin(&writer)) throw std::runtime_error("cannot start PDF painter");
    bool first = true;
    auto line = document.lines.begin();
    for (const auto& page : document.pages) {
      if (!include_title && page.body_number == 0) continue;
      if (!first && !writer.newPage()) throw std::runtime_error("cannot create PDF page");
      first = false;
      if (page.number) paint_text(&painter, page.number->text, page.number->geometry, {}, fonts);
      while (line != document.lines.end() && line->page < page.body_number) ++line;
      while (line != document.lines.end() && line->page == page.body_number) {
        paint_text(&painter, line->text, line->geometry, line->styles, fonts);
        ++line;
      }
    }
    if (!painter.end()) throw std::runtime_error("cannot finish PDF painter");
  }
  if (!output.commit()) throw std::runtime_error(output.errorString().toStdString());
}
}
