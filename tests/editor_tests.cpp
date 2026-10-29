#include "editor/editor_commands.h"
#include "editor/editor_view.h"
#include "editor/screenplay_layout.h"
#include "fountain/import.h"

#include <QClipboard>
#include <QMimeData>
#include <QRawFont>
#include <QTextCursor>
#include <QDir>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QProcess>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextFrame>
#include <QTextLayout>
#include <QtTest>
#include <fstream>
#include <sstream>

using namespace apollo;
using namespace apollo::editor;

namespace {
// Expectations are in profile points; the editor works in 96 dpi pixels.
constexpr double tolerance = 0.01, pt = 1 / px_per_point, page_stride_pt = page_stride * pt;

std::string read(const QString& path) {
  std::ifstream input(path.toStdString());
  std::ostringstream text; text << input.rdbuf();
  return text.str();
}
QString fixture_path(const QString& name, const char* suffix) {
  return QStringLiteral(APOLLO_FIXTURE_DIR "/") + name + suffix;
}

struct Expected { int page, slot; QString element, text; };

// Body lines and page numbers of a hand-derived fixture expectation.
std::vector<Expected> expected_lines(const QString& name) {
  std::vector<Expected> lines;
  int page = 0;
  for (const auto& row : QString::fromStdString(read(fixture_path(name, ".expected"))).split('\n')) {
    if (row.isEmpty() || row.startsWith('#')) continue;
    if (row.startsWith(QStringLiteral("page "))) {
      page = row.section(' ', 1, 1).toInt();
      const auto number = row.section(QStringLiteral("number="), 1);
      if (number != QStringLiteral("none")) lines.push_back({page, 0, QStringLiteral("number"), number});
      continue;
    }
    const auto fields = row.split('\t');
    lines.push_back({page, fields[0].toInt(), fields[1], fields[2]});
  }
  return lines;
}

// Whether the open document has a title page, which is then the first page
// card; body page n is card n - 1 otherwise.
bool title_page = false;

// Independent profile values (docs/export-profile.md) in points.
// Slot 0 is the page-number line: top at 0.5 in, so its baseline is 9 pt
// lower as for body slot 1 (top 1 in, baseline 81 pt); right edge 7.25 in.
double baseline(int page, int slot) {
  return (page - (title_page ? 0 : 1)) * page_stride_pt + (slot ? 81 + (slot - 1) * profile::line_pitch : 45);
}
double profile_x(const Expected& line) {
  const double width = line.text.toUcs4().size() * profile::advance;
  if (line.element.endsWith(QStringLiteral("@center"))) return 306 - width / 2;
  if (line.element == "contact@5.2in") return 374.4;
  if (line.element == "draft-date@1.5in") return 108;
  static const QMap<QString, double> columns{
      {"heading", 108}, {"action", 108}, {"cue", 288}, {"more", 288}, {"dialogue", 216}, {"lyric", 216},
      {"paren", 244.8}, {"paren+", 252}, {"dual-cue-L", 180}, {"dual-cue-R", 403.2},
      {"dual-dialogue-L", 108}, {"dual-dialogue-R", 331.2}, {"dual-paren-L", 136.8}, {"dual-paren-R", 360}};
  if (line.element == "transition") return 511.2 - width;
  if (line.element == "number") return 522 - width;
  if (line.element == "centered") return 324 - width / 2;
  return columns.value(line.element, -1);
}

// A document line as the editor shows it, in points.
struct LaidOut { QString text; double x, baseline, top, height, width; };
std::vector<LaidOut> laid_out_lines(const QTextDocument& document) {
  std::vector<LaidOut> shown;
  for (auto block = document.begin(); block != document.end(); block = block.next()) {
    const auto origin = document.documentLayout()->blockBoundingRect(block).topLeft();
    const auto* layout = block.layout();
    for (int i = 0; i < layout->lineCount(); ++i) {
      const auto line = layout->lineAt(i);
      const auto text = block.text().mid(line.textStart(), line.textLength());
      int lead = 0;
      while (lead < text.size() && text[lead].isSpace()) ++lead;
      shown.push_back({text.trimmed(), (origin.x() + line.cursorToX(line.textStart() + lead)) * pt,
                       (origin.y() + line.y() + line.ascent()) * pt, (origin.y() + line.y()) * pt,
                       line.height() * pt, line.naturalTextWidth() * pt});
    }
  }
  return shown;
}

bool near(double a, double b) { return std::abs(a - b) <= tolerance; }

std::vector<int> styles_per_byte(const Element& element) {
  std::vector<int> flags(element.text.size());
  for (const auto& span : element.styles)
    for (auto i = span.start; i < span.end; ++i) flags[i] |= 1 << static_cast<int>(span.style);
  return flags;
}
bool same_element(const Element& a, const Element& b) {
  return a.type == b.type && a.text == b.text && a.dual_right == b.dual_right && styles_per_byte(a) == styles_per_byte(b);
}
// Type, text, dual flag, and emphasis of every element; the exact document.
bool same_elements(const std::vector<Element>& a, const std::vector<Element>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), same_element);
}

// Observe actual input-method commits from the desktop, not synthetic Qt
// events. Used only by the explicitly enabled native virtual-keyboard check.
struct InputMethodCommits : QObject {
  QString text;
  bool eventFilter(QObject*, QEvent* event) override {
    if (event->type() == QEvent::InputMethod)
      text += static_cast<QInputMethodEvent*>(event)->commitString();
    return false;
  }
};
}

class EditorTests : public QObject {
  Q_OBJECT
  QFont font_;
  std::unique_ptr<QTextDocument> document_;
  std::unique_ptr<QQuickView> view_;
  QQuickItem* editor_ = nullptr;

  ScreenplayLayout* layout() const { return static_cast<ScreenplayLayout*>(document_->documentLayout()); }
  bool open(const QString& name) { return open_elements(import_fountain(read(fixture_path(name, ".fountain")))); }
  bool open_elements(const std::vector<Element>& elements) {
    view_.reset();
    document_ = screenplay_document(elements, font_);
    title_page = layout()->has_title_page();
    view_ = std::make_unique<QQuickView>();
    editor_ = show_editor(*view_, *document_);
    view_->resize(1100, static_cast<int>(2 * page_stride));
    view_->show();
    if (!QTest::qWaitForWindowExposed(view_.get())) return false;
    view_->requestActivate();
    editor_->forceActiveFocus();
    return QTest::qWaitForWindowActive(view_.get()) && editor_->hasActiveFocus() && layout()->error().isEmpty();
  }
  int position_of(const QString& text) const { return document_->toPlainText().indexOf(text); }
  QRectF caret_rect(int position) const {
    QRectF rect;
    QMetaObject::invokeMethod(editor_, "positionToRectangle", Q_RETURN_ARG(QRectF, rect), Q_ARG(int, position));
    return {rect.x() * pt, rect.y() * pt, rect.width() * pt, rect.height() * pt};
  }
  int position_at(double x, double y) const {
    int position = -1;
    QMetaObject::invokeMethod(editor_, "positionAt", Q_RETURN_ARG(int, position),
                              Q_ARG(qreal, x * px_per_point), Q_ARG(qreal, y * px_per_point));
    return position;
  }
  void type(const QString& text) {
    for (const auto character : text) QTest::keyClick(view_.get(), character.toLatin1());
  }
  void press(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) { QTest::keyClick(view_.get(), key, modifiers); }
  QString compositor_keys(const QStringList& keys) {
    if (!view_->isActive()) return QStringLiteral("test window lost focus before virtual keyboard input");
    QProcess process;
    process.start(QStringLiteral("wtype"), keys);
    if (!process.waitForStarted(1000)) return process.errorString();
    if (!QTest::qWaitFor([&] { return process.state() == QProcess::NotRunning; }, 5000)) {
      process.kill(); process.waitForFinished();
      return QStringLiteral("wtype timed out");
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode())
      return QStringLiteral("wtype failed: ") + QString::fromUtf8(process.readAllStandardError());
    return {};
  }
  QTextBlock block_at(int position) const { return document_->findBlock(position); }
  // The glyphs a block's first line draws equal those of the given text.
  bool draws(const QTextBlock& block, const QString& text) const {
    const auto runs = block.layout()->glyphRuns(0, static_cast<int>(text.size()));
    QList<quint32> drawn;
    for (const auto& run : runs) drawn += run.glyphIndexes();
    return drawn == QRawFont::fromFont(font_).glyphIndexesForString(text);
  }
  int cursor() const { return editor_->property("cursorPosition").toInt(); }
  double caret_baseline(int position) const { return caret_rect(position).top() + QFontMetricsF(font_).ascent() * pt; }
  QRect viewport() const {
    const auto* flick = view_->rootObject()->findChild<QQuickItem*>("flick");
    return flick->mapRectToScene(flick->boundingRect()).toAlignedRect().intersected(QRect(QPoint(), view_->size()));
  }
  int highlighted(const QImage& image, const QRectF& document_rect) const {
    const auto region = editor_->mapRectToScene(document_rect).toAlignedRect().intersected(viewport()).intersected(image.rect());
    int count = 0;
    for (int y = region.top(); y <= region.bottom(); ++y)
      for (int x = region.left(); x <= region.right(); ++x)
        count += image.pixelColor(x, y) == QColor("#8fb4e8");
    return count;
  }
  // Pixels darker than the page or background that are outside every
  // expected line, and lines with no ink, in a grab of the current view.
  // Only lines inside the scrolled view must have ink.
  std::pair<int, int> ink_mismatch(const QString& artifact) {
    editor_->setProperty("cursorVisible", false);
    const auto image = view_->grabWindow();
    image.save(QStringLiteral(APOLLO_ARTIFACT_DIR "/") + artifact + ".png");
    std::vector<QRectF> regions;
    for (const auto& line : laid_out_lines(*document_))
      if (!line.text.isEmpty())
        regions.push_back(editor_->mapRectToScene(QRectF(line.x * px_per_point, line.top * px_per_point,
                                 line.width * px_per_point, line.height * px_per_point)).adjusted(-1, -1, 1, 1));
    const QFontMetricsF metrics(font_);
    for (const auto& decoration : layout()->decoration_list())
      regions.push_back(editor_->mapRectToScene(QRectF(decoration.baseline.x(), decoration.baseline.y() - metrics.ascent(),
                               metrics.horizontalAdvance(decoration.text), metrics.height())).adjusted(-1, -1, 1, 1));
    std::vector<bool> inked(regions.size());
    int stray = 0;
    const auto visible = viewport().intersected(image.rect());
    for (int y = visible.top(); y <= visible.bottom(); ++y)
      for (int x = visible.left(); x <= visible.right(); ++x) {
        if (qGray(image.pixel(x, y)) >= 96) continue;
        const QPointF point(x + 0.5, y + 0.5);
        bool inside = false;
        for (std::size_t i = 0; i < regions.size(); ++i)
          if (regions[i].contains(point)) { inked[i] = true; inside = true; }
        stray += !inside;
      }
    int blank = 0;
    for (std::size_t i = 0; i < regions.size(); ++i) blank += !inked[i] && QRectF(visible).contains(regions[i]);
    return {stray, blank};
  }

private slots:
  void initTestCase() {
    // The regular suite is deterministic offscreen. The same interaction
    // checks can also exercise the real Wayland/OpenGL rendering path.
    const bool native = qEnvironmentVariableIntValue("APOLLO_TEST_OPENGL") == 1;
    if (native) QVERIFY2(QGuiApplication::platformName() == QStringLiteral("wayland"), "Native checks require Wayland");
    QQuickWindow::setGraphicsApi(native ? QSGRendererInterface::OpenGL : QSGRendererInterface::Software);
    font_ = courier_prime(QStringLiteral(APOLLO_FONT_DIR));
    QVERIFY(QDir().mkpath(QStringLiteral(APOLLO_ARTIFACT_DIR)));
  }
  // Any warning, such as a QML binding error, fails the test.
  void init() { QTest::failOnWarning(QRegularExpression(QStringLiteral(".*"))); }
  void cleanup() { view_.reset(); document_.reset(); }

  // The approved shell must leave the live editor and its selection intact
  // when the outline is shown/hidden, including a compact desktop window.
  void writing_desk_outline_and_focus() {
    QVERIFY(open_elements(import_fountain(read(QStringLiteral(APOLLO_LOOK_SOURCE)))));
    view_->rootObject()->setProperty("documentTitle", "The Last Broadcast");
    view_->resize(1200, 1000);
    auto* root = view_->rootObject();
    auto* toggle = root->findChild<QQuickItem*>("outlineToggle");
    auto* close = root->findChild<QQuickItem*>("outlineClose");
    auto* outline = root->findChild<QQuickItem*>("sceneOutline");
    QVERIFY(toggle && close && outline);
    QVERIFY(!outline->isVisible());
    const auto original = document_->toPlainText();
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("Then let's")));
    press(Qt::Key_Right, Qt::ShiftModifier);
    const auto selected = editor_->property("selectedText").toString();
    const auto position = cursor();
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/writing-desk-closed.png"));
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, toggle->mapToScene(toggle->boundingRect().center()).toPoint());
    QTRY_VERIFY(outline->isVisible());
    QCOMPARE(outline->width(), 280.0);
    QVERIFY(editor_->hasActiveFocus());
    QCOMPARE(cursor(), position);
    QCOMPARE(editor_->property("selectedText").toString(), selected);
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/writing-desk-open.png"));
    QTest::mouseClick(view_.get(), Qt::LeftButton, Qt::NoModifier, close->mapToScene(close->boundingRect().center()).toPoint());
    QTRY_VERIFY(!outline->isVisible());
    press(Qt::Key_Backslash, Qt::ControlModifier);
    QTRY_VERIFY(outline->isVisible());
    view_->resize(640, 800);
    QTest::qWait(30);
    view_->grabWindow().save(QStringLiteral(APOLLO_ARTIFACT_DIR "/writing-desk-compact.png"));
    const auto caret = editor_->mapRectToScene(editor_->property("cursorRectangle").toRectF());
    QVERIFY(QRectF(viewport()).contains(caret.center()));
    press(Qt::Key_Backslash, Qt::ControlModifier);
    QTRY_VERIFY(!outline->isVisible());
    QCOMPARE(document_->toPlainText(), original);
    QCOMPARE(cursor(), position);
    QCOMPARE(editor_->property("selectedText").toString(), selected);
    QVERIFY(editor_->hasActiveFocus());
  }

  // The document keeps each element's type, text, dual flag, and emphasis.
  void document_round_trips_elements() {
    for (const char* name : {"01-element-geometry", "02-dialogue-split", "03-heading-keep-with-next", "04-dialogue-push",
                             "05-parenthetical-at-break", "06-action-split", "07-title-page-and-manual-break",
                             "08-dual-dialogue-and-contd"}) {
      const auto elements = import_fountain(read(fixture_path(name, ".fountain")));
      const auto document = screenplay_document(elements, font_);
      const auto loaded = document_elements(*document);
      QVERIFY2(same_elements(loaded, elements), name);
    }
  }

  // TextEdit traverses the root frame during render-thread synchronization.
  // It must exist with the document's ownership before that first paint,
  // including after clear() discards the previous document objects.
  void document_prepares_render_objects_data() {
    QTest::addColumn<bool>("clear_first");
    QTest::newRow("loaded") << false;
    QTest::newRow("cleared and refilled") << true;
  }
  void document_prepares_render_objects() {
    QFETCH(bool, clear_first);
    const auto document = screenplay_document({{ElementType::action, "A lamp turns."}}, font_);
    if (clear_first) {
      document->clear();
      QTextCursor cursor(document.get());
      insert_elements(cursor, {{ElementType::action, "The room is empty."}});
    }
    // findChild does not lazily create a frame, unlike rootFrame().
    const auto* frame = document->findChild<QTextFrame*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(frame);
    QCOMPARE(frame->parent(), document.get());
    QCOMPARE(frame->thread(), document->thread());
    QCOMPARE(frame->firstPosition(), 0);
    QCOMPARE(frame->lastPosition(), document->characterCount() - 1);
  }

  // Each fixture line appears at its hand-derived page, slot, and profile
  // column; fixture 07's title page is the first page card. Generated text is
  // a decoration and never document text.
  void lines_follow_fixture_pages_data() {
    QTest::addColumn<QString>("name");
    for (const char* name : {"01-element-geometry", "02-dialogue-split", "03-heading-keep-with-next", "04-dialogue-push",
                             "05-parenthetical-at-break", "06-action-split", "07-title-page-and-manual-break",
                             "08-dual-dialogue-and-contd"})
      QTest::newRow(name) << name;
  }
  void lines_follow_fixture_pages() {
    QFETCH(QString, name);
    QVERIFY(open(name));
    auto shown = laid_out_lines(*document_);
    auto decorations = layout()->decoration_list();
    std::vector<bool> used_line(shown.size()), used_decoration(decorations.size());
    auto take_decoration = [&](const QString& text, double x, double y) {
      for (std::size_t i = 0; i < decorations.size(); ++i)
        if (!used_decoration[i] && decorations[i].text == text && near(decorations[i].baseline.x() * pt, x) &&
            near(decorations[i].baseline.y() * pt, y)) { used_decoration[i] = true; return true; }
      return false;
    };
    for (const auto& want : expected_lines(name)) {
      const double x = profile_x(want), y = baseline(want.page, want.slot);
      QVERIFY2(x >= 0, qPrintable(want.element));
      bool found = false;
      for (std::size_t i = 0; i < shown.size() && !found; ++i) {
        if (used_line[i] || !near(shown[i].x, x) || !near(shown[i].baseline, y) || shown[i].text.isEmpty()) continue;
        // A title is shown uppercase; its document text is what was typed.
        if (shown[i].text == want.text || (want.element == QStringLiteral("title@center") && shown[i].text.toUpper() == want.text) ||
            (want.text.startsWith(shown[i].text) &&
             take_decoration(want.text.mid(shown[i].text.size()), x + shown[i].text.toUcs4().size() * profile::advance, y)))
          found = used_line[i] = true;
      }
      if (!found) found = take_decoration(want.text, x, y);
      QVERIFY2(found, qPrintable(QString("%1 %2:%3 %4").arg(name).arg(want.page).arg(want.slot).arg(want.text)));
    }
    for (std::size_t i = 0; i < shown.size(); ++i) QVERIFY2(used_line[i] || shown[i].text.isEmpty(), qPrintable(shown[i].text));
    QVERIFY(std::all_of(used_decoration.begin(), used_decoration.end(), [](bool used) { return used; }));
    const auto text = document_->toPlainText();
    QVERIFY(!text.contains(QStringLiteral("(MORE)")) && !text.contains(QStringLiteral("CONT’D")));
  }

  // Glyphs inside each editor line use the engine's fixed advance, so
  // editor and PDF columns agree within a line, not just at its start.
  void glyph_advances_match_profile() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    int checked = 0;
    for (auto block = document_->begin(); block != document_->end(); block = block.next()) {
      const auto* layout = block.layout();
      for (int i = 0; i < layout->lineCount(); ++i) {
        const auto line = layout->lineAt(i);
        for (int k = 0; k <= line.textLength(); ++k, ++checked)
          QVERIFY(near((line.cursorToX(line.textStart() + k) - line.cursorToX(line.textStart())) * pt, k * profile::advance));
      }
    }
    QVERIFY(checked > 100);
  }

  void textedit_maps_positions_across_page_gap() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    const int falling = position_of(QStringLiteral("falling. The lamp")), somewhere = position_of(QStringLiteral("Somewhere"));
    QVERIFY(near(caret_rect(falling).x(), 108));
    QVERIFY(near(caret_baseline(falling), baseline(1, 55)));
    QVERIFY(near(caret_rect(somewhere).x(), 108));
    QVERIFY(near(caret_baseline(somewhere), baseline(2, 1)));
    QVERIFY(near(caret_rect(somewhere + 4).x(), 108 + 4 * profile::advance));
    QCOMPARE(position_at(108 + 4.4 * profile::advance, baseline(2, 1) - 3), somewhere + 4);
    QCOMPARE(position_at(108 + 2.4 * profile::advance, baseline(1, 55) - 3), falling + 2);
    // A press in the gap between pages or in the blank slots below the last
    // page-1 line goes to the nearest line.
    QCOMPARE(position_at(108, profile::page_height + page_gap * pt / 2), falling);
    QCOMPARE(position_at(108, page_stride_pt + 40), somewhere);
  }

  void caret_moves_across_page_gap() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    const int somewhere = position_of(QStringLiteral("Somewhere")), falling = position_of(QStringLiteral("falling. The lamp"));
    editor_->setProperty("cursorPosition", somewhere - 1);
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 55)));
    QTest::keyClick(view_.get(), Qt::Key_Right);
    QCOMPARE(cursor(), somewhere);
    QVERIFY(near(caret_baseline(cursor()), baseline(2, 1)));
    editor_->setProperty("cursorPosition", falling + 5);
    QTest::keyClick(view_.get(), Qt::Key_Down);
    QCOMPARE(cursor(), somewhere + 5);
    QTest::keyClick(view_.get(), Qt::Key_Up);
    QCOMPARE(cursor(), falling + 5);
    QTest::keyClick(view_.get(), Qt::Key_Down);
    QTest::keyClick(view_.get(), Qt::Key_End);
    QCOMPARE(cursor(), position_of(QStringLiteral(" to lean into")));
    QTest::keyClick(view_.get(), Qt::Key_Home);
    QCOMPARE(cursor(), somewhere);
    QTest::keyClick(view_.get(), Qt::Key_Left);
    QCOMPARE(cursor(), somewhere - 1);
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 55)));
  }

  // Typing at the start of page 2 changes the split. Each keystroke leaves
  // the caret right after the typed character; the final split is the hand-
  // derived latest sentence break that leaves two lines on page 2.
  void typing_at_page_boundary_reflows() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    const int somewhere = position_of(QStringLiteral("Somewhere"));
    const auto original = document_->toPlainText();
    editor_->setProperty("cursorPosition", somewhere);
    const QString typed = QStringLiteral("Lightning. ");
    for (int i = 0; i < typed.size(); ++i) {
      type(typed.mid(i, 1));
      QCOMPARE(cursor(), somewhere + i + 1);
      QVERIFY(layout()->error().isEmpty());
      // The caret stays on page 2's first line just after the typed text
      // until the final space lets "Lightning." move to page 1.
      const int column = i + 1 < typed.size() ? i + 1 : 0;
      QVERIFY2(near(caret_baseline(cursor()), baseline(2, 1)) && near(caret_rect(cursor()).x(), 108 + column * profile::advance),
               qPrintable(QString("caret jumped after %1").arg(typed.left(i + 1))));
    }
    QVERIFY(document_->toPlainText().contains(QStringLiteral("sputters. Lightning. Somewhere")));
    const int lightning = position_of(QStringLiteral("Lightning."));
    QVERIFY(near(caret_baseline(lightning), baseline(1, 55)));
    QVERIFY(near(caret_rect(lightning).x(), 108 + 28 * profile::advance));
    QVERIFY(near(caret_baseline(cursor()), baseline(2, 1)));
    QVERIFY(near(caret_rect(cursor()).x(), 108));
    QTest::keyClick(view_.get(), Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(document_->toPlainText(), original);
    QVERIFY(near(caret_baseline(position_of(QStringLiteral("Somewhere"))), baseline(2, 1)));
  }

  void selection_across_page_gap_copies_text() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    const int falling = position_of(QStringLiteral("falling. The lamp"));
    editor_->setProperty("cursorPosition", falling);
    QTest::keyClick(view_.get(), Qt::Key_Down, Qt::ShiftModifier);
    QTest::keyClick(view_.get(), Qt::Key_Down, Qt::ShiftModifier);
    const QString expected = QStringLiteral("falling. The lamp sputters. Somewhere below them, glass breaks and the whole tower seems ");
    QCOMPARE(editor_->property("selectedText").toString(), expected);
    QTest::keyClick(view_.get(), Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QGuiApplication::clipboard()->text(), expected);
    editor_->setProperty("cursorVisible", false);
    const auto image = view_->grabWindow();
    image.save(QStringLiteral(APOLLO_ARTIFACT_DIR "/selection-across-gap.png"));
    // Highlight rows from just above each baseline, below the previous line.
    // A native compositor may limit the requested two-page window's height;
    // account for the resulting scroll when inspecting the captured pixels.
    auto highlighted = [&](double top, double bottom) {
      return this->highlighted(image, QRectF(144, top * px_per_point, 200, (bottom - top) * px_per_point));
    };
    QVERIFY(highlighted(baseline(1, 55) - 5, baseline(1, 55)) > 100);
    QVERIFY(highlighted(baseline(2, 1) - 5, baseline(2, 1)) > 100);
    QCOMPARE(highlighted(profile::page_height, page_stride_pt), 0);
    QCOMPARE(highlighted(baseline(2, 2) - 5, baseline(2, 2)), 0);
  }

  // Lengthening an early action pushes later lines down and moves the split.
  // TextEdit shifts unchanged text nodes by one offset after an edit, which
  // would misplace repaginated blocks if it did not rebuild them all.
  void rendering_follows_repagination() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    QCOMPARE(ink_mismatch(QStringLiteral("before-repagination")), std::make_pair(0, 0));
    const auto split_before = caret_baseline(position_of(QStringLiteral("Somewhere")));
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("Filler line 01.")) + 15);
    // Three lines leave four slots at the bottom of page 1, so the latest
    // valid sentence break is after "chair." and page 2 starts with "Theo".
    type(QStringLiteral(" The wind keeps rising over the rocks, and every window in the lamp room rattles"
                        " in its frame while the great lamp turns and turns."));
    QVERIFY(layout()->error().isEmpty());
    QCOMPARE(view_->rootObject()->findChild<QQuickItem*>("flick")->property("contentY").toDouble(), 0.0);
    QVERIFY(near(caret_baseline(position_of(QStringLiteral("Theo drags"))), baseline(2, 1)));
    QVERIFY(!near(caret_baseline(position_of(QStringLiteral("Somewhere"))), split_before));
    QCOMPARE(ink_mismatch(QStringLiteral("after-repagination")), std::make_pair(0, 0));
  }

  // A new cue without dialogue yet is laid out alone (an engine rule), so
  // pagination never stops while a speech is typed. Slots follow fixture 02:
  // its last action is at page 2 slot 6.
  void incomplete_speech_stays_paginated() {
    QVERIFY(open(QStringLiteral("02-dialogue-split")));
    editor_->setProperty("cursorPosition", document_->characterCount() - 1);
    press(Qt::Key_Return);
    QVERIFY(near(caret_baseline(cursor()), baseline(2, 8)));
    press(Qt::Key_Tab);
    QCOMPARE(element_type(block_at(cursor())), ElementType::cue);
    QVERIFY(near(caret_rect(cursor()).x(), 288));
    type(QStringLiteral("theo"));
    QVERIFY(layout()->error().isEmpty());
    QVERIFY(document_->toPlainText().endsWith(QStringLiteral("theo")));
    QVERIFY(draws(block_at(cursor()), QStringLiteral("THEO")));
    QVERIFY(near(caret_baseline(cursor()), baseline(2, 8)));
    press(Qt::Key_Return);
    QCOMPARE(element_type(block_at(cursor())), ElementType::dialogue);
    QVERIFY(near(caret_baseline(cursor()), baseline(2, 9)) && near(caret_rect(cursor()).x(), 216));
    type(QStringLiteral("Nobody."));
    QVERIFY(layout()->error().isEmpty());
    QVERIFY(near(caret_baseline(position_of(QStringLiteral("Nobody."))), baseline(2, 9)));
  }

  // A composing input method inserts preedit text into the block's
  // QTextLayout, not the document. It must not break the engine mapping.
  void input_method_preedit_at_page_gap() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    const int somewhere = position_of(QStringLiteral("Somewhere"));
    editor_->setProperty("cursorPosition", somewhere);
    QInputMethodEvent preedit(QStringLiteral("´"), {});
    QCoreApplication::sendEvent(editor_, &preedit);
    QCOMPARE(editor_->property("preeditText").toString(), QStringLiteral("´"));
    QVERIFY(layout()->error().isEmpty());
    QVERIFY(near(caret_baseline(somewhere), baseline(2, 1)));
    QInputMethodEvent commit;
    commit.setCommitString(QStringLiteral("é"));
    QCoreApplication::sendEvent(editor_, &commit);
    QVERIFY(document_->toPlainText().contains(QStringLiteral("sputters. éSomewhere")));
    QCOMPARE(cursor(), somewhere + 1);
    QVERIFY(layout()->error().isEmpty());
    QVERIFY(near(caret_baseline(cursor()), baseline(2, 1)));
    QVERIFY(near(caret_rect(cursor()).x(), 108 + profile::advance));
  }

  void wayland_input_method_at_page_gap_data() {
    QTest::addColumn<QStringList>("keys");
    QTest::newRow("compose") << QStringList{"-k", "Multi_key", "-k", "apostrophe", "-k", "e"};
    QTest::newRow("dead key") << QStringList{"-k", "dead_acute", "-k", "e"};
  }
  void wayland_input_method_at_page_gap() {
    if (qEnvironmentVariableIntValue("APOLLO_TEST_WAYLAND_INPUT") != 1)
      QSKIP("Enable APOLLO_TEST_WAYLAND_INPUT=1 in a native session with wtype and fcitx5");
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("wayland"));
    QCOMPARE(qEnvironmentVariable("QT_IM_MODULE"), QStringLiteral("fcitx"));
    QFETCH(QStringList, keys);
    QVERIFY(open(QStringLiteral("06-action-split")));
    const auto original = document_->toPlainText();
    const int somewhere = position_of(QStringLiteral("Somewhere"));
    editor_->setProperty("cursorPosition", somewhere);
    InputMethodCommits commits;
    editor_->installEventFilter(&commits);
    const auto error = compositor_keys(keys);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QTRY_COMPARE(document_->toPlainText(), original.left(somewhere) + QStringLiteral("é") + original.mid(somewhere));
    QCOMPARE(commits.text, QStringLiteral("é"));
    QCOMPARE(cursor(), somewhere + 1);
    QVERIFY(layout()->error().isEmpty());
    QVERIFY(near(caret_baseline(cursor()), baseline(2, 1)));
    QVERIFY(near(caret_rect(cursor()).x(), 108 + profile::advance));
  }

  // Rows of the brief's Enter table on fixture 01: at the end of an element,
  // a new element of the listed type follows it.
  void enter_at_element_end_data() {
    QTest::addColumn<QString>("end");
    QTest::addColumn<int>("type");
    QTest::newRow("action") << QStringLiteral("out into the cold.") << static_cast<int>(ElementType::action);
    QTest::newRow("cue") << QStringLiteral("MARA") << static_cast<int>(ElementType::dialogue);
    QTest::newRow("parenthetical in a speech") << QStringLiteral("turning around)") << static_cast<int>(ElementType::dialogue);
    QTest::newRow("dialogue") << QStringLiteral("always leave the lights on.") << static_cast<int>(ElementType::action);
    QTest::newRow("transition") << QStringLiteral("CUT TO:") << static_cast<int>(ElementType::action);
  }
  void enter_at_element_end() {
    QFETCH(QString, end);
    QFETCH(int, type);
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const int blocks = document_->blockCount();
    editor_->setProperty("cursorPosition", position_of(end) + end.size());
    press(Qt::Key_Return);
    QCOMPARE(document_->blockCount(), blocks + 1);
    QCOMPARE(static_cast<int>(element_type(block_at(cursor()))), type);
    QVERIFY(block_at(cursor()).text().isEmpty() && block_at(cursor()).position() == cursor());
    QVERIFY(layout()->error().isEmpty());
  }

  // A new empty action keeps its own slot (6, after the action at 3-4), so
  // the caret never overlaps text and the cue moves from slot 6 to 8.
  // Enter on an empty element makes it action; Shift+Enter is Enter.
  void empty_elements_and_enter() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const QString cold = QStringLiteral("out into the cold.");
    editor_->setProperty("cursorPosition", position_of(cold) + cold.size());
    press(Qt::Key_Return, Qt::ShiftModifier);
    QVERIFY(!document_->toPlainText().contains(QChar::LineSeparator));
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 6)) && near(caret_rect(cursor()).x(), 108));
    QVERIFY(near(caret_baseline(position_of(QStringLiteral("MARA"))), baseline(1, 8)));
    QCOMPARE(ink_mismatch(QStringLiteral("empty-action")), std::make_pair(0, 0));
    const int blocks = document_->blockCount();
    press(Qt::Key_Tab);
    QCOMPARE(element_type(block_at(cursor())), ElementType::cue);
    press(Qt::Key_Return);
    QCOMPARE(element_type(block_at(cursor())), ElementType::action);
    QCOMPARE(document_->blockCount(), blocks);
    // An emptied parenthetical under a cue returns to dialogue in one undo
    // step, without losing its element identity or adding a block.
    auto parenthetical = document_->begin();
    while (parenthetical.isValid() && element_type(parenthetical) != ElementType::parenthetical) parenthetical = parenthetical.next();
    QVERIFY(parenthetical.isValid());
    const auto original = block_element(parenthetical);
    QMetaObject::invokeMethod(editor_, "select", Q_ARG(int, parenthetical.position()),
                              Q_ARG(int, parenthetical.position() + parenthetical.length() - 1));
    press(Qt::Key_Backspace);
    press(Qt::Key_Return);
    QCOMPARE(element_type(block_at(cursor())), ElementType::dialogue);
    QCOMPARE(block_element(block_at(cursor())).id, original.id);
    QCOMPARE(document_->blockCount(), blocks);
    press(Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(element_type(document_->findBlock(parenthetical.position())), ElementType::parenthetical);
    press(Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(block_element(document_->findBlock(parenthetical.position())), original);
  }

  // Enter inside dialogue keeps one speech element and an authored break.
  void enter_inside_element_splits_it() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const int at = position_of(QStringLiteral("I always leave"));
    editor_->setProperty("cursorPosition", at);
    press(Qt::Key_Return);
    QCOMPARE(block_at(at), block_at(cursor()));
    QCOMPARE(block_element(block_at(at)).text, std::string("I left the lights on for you. \nI always leave the lights on."));
    QCOMPARE(element_type(block_at(at)), ElementType::dialogue);
    QCOMPARE(element_type(block_at(cursor())), ElementType::dialogue);
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 10)) && near(caret_rect(cursor()).x(), 216));
  }

  // Enter before a space leaves it at the start of the authored line. The
  // engine wraps the visible text (wrap() skips leading spaces), so the line
  // after the break shows "I always ..." at the dialogue column, slot 10, as
  // fixture 01 does; the rest of the page keeps its slots. The space stays in
  // the document, to the left of the column, and the caret can reach it.
  void enter_before_space_keeps_engine_breaks() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const int space = position_of(QStringLiteral(" I always leave"));
    editor_->setProperty("cursorPosition", space);
    press(Qt::Key_Return);
    QCOMPARE(block_element(block_at(space)).text, std::string("I left the lights on for you.\n I always leave the lights on."));
    QVERIFY2(layout()->error().isEmpty(), qPrintable(layout()->error()));
    const auto block = block_at(space);
    QCOMPARE(block.layout()->lineCount(), 2);
    QCOMPARE(block.layout()->lineAt(1).textStart(), space - block.position() + 1);
    std::vector<std::pair<QString, double>> expected{{"I left the lights on for you.", baseline(1, 9)},
                                                     {"I always leave the lights on.", baseline(1, 10)}};
    for (const auto& [text, y] : expected) {
      const auto lines = laid_out_lines(*document_);
      const auto line = std::find_if(lines.begin(), lines.end(), [&](const LaidOut& l) { return l.text == text; });
      QVERIFY2(line != lines.end(), qPrintable(text));
      QVERIFY2(near(line->x, 216) && near(line->baseline, y), qPrintable(text));
    }
    QCOMPARE(cursor(), space + 1);
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 10)) && near(caret_rect(cursor()).x(), 216 - profile::advance));
    QVERIFY(near(caret_rect(cursor() + 1).x(), 216));
    QVERIFY(near(caret_baseline(position_of(QStringLiteral("THEO"))), baseline(1, 12)));
    QVERIFY(near(caret_baseline(position_of(QStringLiteral("CUT TO:"))), baseline(1, 18)));
    QCOMPARE(layout()->pageCount(), 1);
    QCOMPARE(ink_mismatch(QStringLiteral("enter-before-space")), std::make_pair(0, 0));
    // Typing on either side of the hidden space keeps the mapping.
    type(QStringLiteral("X"));
    QVERIFY2(layout()->error().isEmpty(), qPrintable(layout()->error()));
    QVERIFY(near(caret_rect(cursor()).x(), 216 + profile::advance));
    press(Qt::Key_Backspace);
    press(Qt::Key_Right);
    QVERIFY(near(caret_rect(cursor()).x(), 216));
    type(QStringLiteral("Y"));
    QCOMPARE(block_element(block_at(space)).text, std::string("I left the lights on for you.\n YI always leave the lights on."));
    QVERIFY2(layout()->error().isEmpty(), qPrintable(layout()->error()));
    QVERIFY(near(caret_rect(cursor()).x(), 216 + profile::advance));
  }

  void deleting_whole_element_keeps_surviving_identity() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const auto first = document_->begin(), second = first.next();
    const auto survivor = block_element(second);
    QMetaObject::invokeMethod(editor_, "select", Q_ARG(int, first.position()), Q_ARG(int, second.position()));
    press(Qt::Key_Delete);
    QCOMPARE(block_element(document_->begin()), survivor);
    press(Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(block_element(document_->begin().next()), survivor);
  }

  // Presentation converts ASCII letters only (docs/case-handling.md): é in a
  // heading is drawn as written, and the scene list agrees with the page.
  void presentation_capitalizes_ascii_letters_only() {
    QVERIFY(open_elements({{ElementType::heading, "int. caf\xc3\xa9 - day"}, {ElementType::action, "Rain."}}));
    QVERIFY(draws(document_->begin(), QString::fromUtf8("INT. CAF\xc3\xa9 - DAY")));
    QCOMPARE(view_->rootObject()->property("outlineScenes").toStringList(), QStringList{QString::fromUtf8("INT. CAF\xc3\xa9 - DAY")});
    QCOMPARE(block_element(document_->begin()).text, std::string("int. caf\xc3\xa9 - day"));
  }

  // Tab cycles heading, action, character, parenthetical, dialogue,
  // transition; Shift+Tab goes back. A heading is shown uppercase while the
  // document keeps what was typed, and each change is one undo step.
  void tab_cycles_element_types() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const int at = position_of(QStringLiteral("Mara crosses"));
    editor_->setProperty("cursorPosition", at);
    for (const auto expected : {ElementType::cue, ElementType::parenthetical, ElementType::dialogue,
                                ElementType::transition, ElementType::heading, ElementType::action}) {
      press(Qt::Key_Tab);
      QCOMPARE(element_type(block_at(at)), expected);
      QVERIFY(layout()->error().isEmpty());
    }
    press(Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(element_type(block_at(at)), ElementType::heading);
    QVERIFY(block_at(at).text().startsWith(QStringLiteral("Mara crosses the room,")));
    QVERIFY(draws(block_at(at), QStringLiteral("MARA CROSSES THE ROOM,")));
    press(Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(element_type(block_at(at)), ElementType::action);
    QVERIFY(draws(block_at(at), QStringLiteral("Mara crosses the room,")));
  }

  // A type change redraws every moved line: a heading needs two blank slots
  // instead of one, so fixture 06's later lines move down one slot.
  void type_change_redraws_moved_lines() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    const auto next_before = caret_baseline(position_of(QStringLiteral("Filler line 03.")));
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("Filler line 02.")));
    press(Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(element_type(block_at(cursor())), ElementType::heading);
    QVERIFY(near(caret_baseline(position_of(QStringLiteral("Filler line 03."))), next_before + profile::line_pitch));
    QCOMPARE(ink_mismatch(QStringLiteral("type-change")), std::make_pair(0, 0));
  }

  // Plain text becomes action paragraphs at the caret, one per non-empty
  // line; line separators and tabs never reach the document.
  void paste_plain_text_as_action() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const auto before = document_elements(*document_);
    const QString cold = QStringLiteral("out into the cold.");
    editor_->setProperty("cursorPosition", position_of(cold) + cold.size());
    QGuiApplication::clipboard()->setText(QString::fromUtf8("First line.\n\nSecond line.\r\nThird line.\tEnd"));
    press(Qt::Key_V, Qt::ControlModifier);
    const auto after = document_elements(*document_);
    QCOMPARE(after.size(), before.size() + 4);
    QVERIFY(same_element(after[1], before[1]));
    const std::vector<std::string> pasted{"First line.", "Second line.", "Third", "line. End"};
    for (std::size_t i = 0; i < pasted.size(); ++i)
      QVERIFY(after[2 + i].type == ElementType::action && after[2 + i].text == pasted[i]);
    QVERIFY(same_element(after[6], before[2]));
    QCOMPARE(cursor(), position_of(QStringLiteral("line. End")) + 9);
    QVERIFY(layout()->error().isEmpty());
    press(Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(same_elements(document_elements(*document_), before));
  }

  // Inside dialogue, pasted paragraphs stay dialogue; one line joins the
  // element at the caret.
  void paste_plain_text_in_dialogue_and_inline() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const QString lights = QStringLiteral("always leave the lights on.");
    editor_->setProperty("cursorPosition", position_of(lights) + lights.size());
    QGuiApplication::clipboard()->setText(QStringLiteral("One.\nTwo."));
    press(Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(element_type(block_at(position_of(QStringLiteral("One.")))), ElementType::dialogue);
    QCOMPARE(element_type(block_at(position_of(QStringLiteral("Two.")))), ElementType::dialogue);
    QCOMPARE(block_at(position_of(QStringLiteral("One."))).text(), QStringLiteral("One."));
    const int blocks = document_->blockCount();
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("tall window")));
    QGuiApplication::clipboard()->setText(QStringLiteral("very "));
    press(Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(document_->blockCount(), blocks);
    QVERIFY(document_->toPlainText().contains(QStringLiteral("unlatches the very tall window")));
  }

  // An Apollo selection keeps element types and emphasis through copy/paste.
  void copy_and_paste_keep_types_and_emphasis() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const auto before = document_elements(*document_);
    const auto cue = std::find_if(before.begin(), before.end(), [](const Element& e) { return e.text == "THEO (V.O.)"; });
    QVERIFY(cue != before.end() && cue[1].styles.size() == 3);
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("THEO (V.O.)")));
    press(Qt::Key_Down, Qt::ShiftModifier);
    press(Qt::Key_Down, Qt::ShiftModifier);
    press(Qt::Key_End, Qt::ShiftModifier);
    press(Qt::Key_C, Qt::ControlModifier);
    QVERIFY(QGuiApplication::clipboard()->mimeData()->hasFormat(EditorCommands::mime_type));
    QCOMPARE(QGuiApplication::clipboard()->text(), QString::fromStdString(cue[0].text + "\n" + cue[1].text));
    const QString cold = QStringLiteral("out into the cold.");
    editor_->setProperty("cursorPosition", position_of(cold) + cold.size());
    press(Qt::Key_V, Qt::ControlModifier);
    const auto after = document_elements(*document_);
    QCOMPARE(after.size(), before.size() + 2);
    QVERIFY(same_element(after[2], cue[0]) && same_element(after[3], cue[1]));
    QVERIFY(layout()->error().isEmpty());
  }

  // A single Apollo element keeps its block properties in an empty element
  // or between the split halves of an element with a different type.
  void paste_single_apollo_element_keeps_type_data() {
    QTest::addColumn<QString>("target");
    QTest::newRow("empty") << QString();
    QTest::newRow("inside action") << QStringLiteral("She waits.");
  }
  void paste_single_apollo_element_keeps_type() {
    QFETCH(QString, target);
    const Element cue{ElementType::cue, "MARA", true, {{0, 4, Style::bold}}};
    QVERIFY(open_elements({cue, {ElementType::action, target.toStdString()}}));
    const auto before = document_elements(*document_);
    editor_->setProperty("cursorPosition", 0);
    press(Qt::Key_End, Qt::ShiftModifier);
    press(Qt::Key_C, Qt::ControlModifier);
    editor_->setProperty("cursorPosition", document_->lastBlock().position() + (target.isEmpty() ? 0 : 4));
    press(Qt::Key_V, Qt::ControlModifier);
    const auto after = document_elements(*document_);
    const std::vector<Element> expected = target.isEmpty() ? std::vector<Element>{cue, cue}
        : std::vector<Element>{cue, {ElementType::action, "She "}, cue, {ElementType::action, "waits."}};
    QVERIFY(same_elements(after, expected));
    QVERIFY(layout()->error().isEmpty());
    press(Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(same_elements(document_elements(*document_), before));
    press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(same_elements(document_elements(*document_), after));
  }

  void clipboard_preserves_deliberate_breaks_data() {
    QTest::addColumn<int>("row");
    QTest::newRow("whole element") << 0;
    QTest::newRow("partial Unicode selection after a break") << 1;
  }
  void clipboard_preserves_deliberate_breaks() {
    QFETCH(int, row);
    const Element input = row == 0
        ? Element{ElementType::dialogue, "one\ntwo", false, {{4, 7, Style::bold}}}
        : Element{ElementType::dialogue, "pré\n𝄞 café\ntail", false, {{5, 15, Style::italic}, {10, 15, Style::bold}}};
    const Element expected = row == 0 ? input
        : Element{ElementType::dialogue, "𝄞 café\n", false, {{0, 10, Style::italic}, {5, 10, Style::bold}}};
    const int start = row == 0 ? 0 : 4, end = row == 0 ? 7 : 12;
    const auto source = screenplay_document({input}, font_);
    const auto original = document_elements(*source);
    EditorCommands commands(source.get());
    commands.copy(start, end);
    QCOMPARE(QGuiApplication::clipboard()->text(), QString::fromStdString(expected.text));

    // Apollo paste keeps an internal break, its type, and its emphasis in
    // one block; it must not fall back to external paragraph paste.
    const auto target = screenplay_document({{ElementType::action, ""}}, font_);
    EditorCommands paste(target.get());
    paste.paste(0, 0, 0);
    QCOMPARE(target->blockCount(), 1);
    QVERIFY(same_element(document_elements(*target).front(), expected));
    commands.cut(start, end);
    QCOMPARE(document_elements(*source).front().text, row == 0 ? std::string() : std::string("pré\ntail"));
    source->undo();
    QVERIFY(document_elements(*source) == original);
  }

  // Splitting a destination for a different pasted type uses the same
  // source-byte offsets as copy/cut, including before an internal break.
  void paste_between_halves_with_deliberate_breaks() {
    const auto source = screenplay_document({{ElementType::cue, "MARA"}}, font_);
    EditorCommands copy(source.get());
    copy.copy(0, 4);
    const auto target = screenplay_document({{ElementType::action, "a\nbc"}}, font_);
    const auto original = document_elements(*target);
    EditorCommands paste(target.get());
    paste.paste(3, 3, 3);
    QVERIFY(same_elements(document_elements(*target), {{ElementType::action, "a\nb"},
                         {ElementType::cue, "MARA"}, {ElementType::action, "c"}}));
    target->undo();
    QVERIFY(document_elements(*target) == original);
  }

  // Clipboard data is external. Invalid Apollo metadata must fall back to
  // the clipboard's plain text instead of creating invalid elements/spans.
  void paste_invalid_apollo_metadata_data() {
    QTest::addColumn<QByteArray>("payload");
    QTest::newRow("unknown type") << QByteArray(R"([{"type":99,"text":"Bad","dual":false,"styles":[]}])");
    QTest::newRow("span beyond text") << QByteArray(R"([{"type":2,"text":"Bad","dual":false,"styles":[[0,99,1]]}])");
    QTest::newRow("negative span") << QByteArray(R"([{"type":2,"text":"Bad","dual":false,"styles":[[-1,2,1]]}])");
    QTest::newRow("split utf8") << QByteArray(QStringLiteral(R"([{"type":2,"text":"é","dual":false,"styles":[[0,1,1]]}])").toUtf8());
    QTest::newRow("carriage return in element") << QByteArray(R"([{"type":2,"text":"Bad\rcue","dual":false,"styles":[]}])");
  }
  void paste_invalid_apollo_metadata() {
    QFETCH(QByteArray, payload);
    QVERIFY(open_elements({{ElementType::action, "Start."}}));
    auto* data = new QMimeData;
    data->setText(QStringLiteral(" Fallback."));
    data->setData(EditorCommands::mime_type, payload);
    QGuiApplication::clipboard()->setMimeData(data);
    editor_->setProperty("cursorPosition", 6);
    press(Qt::Key_V, Qt::ControlModifier);
    QVERIFY(same_elements(document_elements(*document_), {{ElementType::action, "Start. Fallback."}}));
    press(Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(document_->toPlainText(), QStringLiteral("Start."));
  }

  // Typing is undone one word at a time, each step restoring the exact text;
  // redo replays the words.
  void typing_undoes_word_by_word() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const QString cold = QStringLiteral("out into the cold.");
    const int end = position_of(cold) + cold.size();
    const auto original = document_->toPlainText();
    auto with = [&](const QString& typed) { return original.left(end) + typed + original.mid(end); };
    editor_->setProperty("cursorPosition", end);
    type(QStringLiteral(" Then she waits."));
    QCOMPARE(document_->toPlainText(), with(QStringLiteral(" Then she waits.")));
    for (const char* typed : {" Then she ", " Then ", " ", ""}) {
      press(Qt::Key_Z, Qt::ControlModifier);
      QCOMPARE(document_->toPlainText(), with(QString::fromLatin1(typed)));
    }
    for (const char* typed : {" ", " Then ", " Then she ", " Then she waits."}) {
      press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
      QCOMPARE(document_->toPlainText(), with(QString::fromLatin1(typed)));
    }
    // Moving the caret starts a new step, even inside a word.
    const auto typed = document_->toPlainText();
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("waits.")) + 2);
    type(QStringLiteral("x"));
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("she ")) + 1);
    type(QStringLiteral("y"));
    press(Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(document_->toPlainText(), QString(typed).replace(QStringLiteral("waits."), QStringLiteral("waxits.")));
    press(Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(document_->toPlainText(), typed);
  }

  // The scene move command puts a whole scene above the previous one as one
  // undo step that restores the exact document.
  void scene_move_and_undo() {
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const auto original = document_elements(*document_);
    const auto second = std::find_if(original.begin() + 1, original.end(), [](const Element& e) { return e.type == ElementType::heading; });
    std::vector<Element> moved(second, original.end());
    moved.insert(moved.end(), original.begin(), second);
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("EXT. LIGHTHOUSE GALLERY")) + 4);
    press(Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier);
    QVERIFY(same_elements(document_elements(*document_), moved));
    QCOMPARE(cursor(), 4);
    QVERIFY(near(caret_baseline(0), baseline(1, 1)));
    QVERIFY(layout()->error().isEmpty());
    press(Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(same_elements(document_elements(*document_), original));
    press(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(same_elements(document_elements(*document_), moved));
    // The first scene has no previous scene to move above.
    editor_->setProperty("cursorPosition", 0);
    press(Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier);
    QVERIFY(same_elements(document_elements(*document_), moved));
  }

  // Exercise the actual compositor/input route for the shortcut the owner
  // reported as doing nothing, with the caret in a known second scene.
  void wayland_scene_shortcut_and_undo() {
    if (qEnvironmentVariableIntValue("APOLLO_TEST_WAYLAND_INPUT") != 1)
      QSKIP("Enable APOLLO_TEST_WAYLAND_INPUT=1 in a native session with wtype");
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("wayland"));
    QVERIFY(open(QStringLiteral("01-element-geometry")));
    const auto original = document_elements(*document_);
    const auto second = std::find_if(original.begin() + 1, original.end(), [](const Element& e) { return e.type == ElementType::heading; });
    std::vector<Element> moved(second, original.end());
    moved.insert(moved.end(), original.begin(), second);
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("EXT. LIGHTHOUSE GALLERY")) + 4);
    auto error = compositor_keys({"-M", "alt", "-M", "shift", "-k", "Up", "-m", "shift", "-m", "alt"});
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QTRY_VERIFY(same_elements(document_elements(*document_), moved));
    QCOMPARE(cursor(), 4);
    error = compositor_keys({"-M", "ctrl", "-k", "z", "-m", "ctrl"});
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QTRY_VERIFY(same_elements(document_elements(*document_), original));
    error = compositor_keys({"-M", "ctrl", "-M", "shift", "-k", "z", "-m", "shift", "-m", "ctrl"});
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QTRY_VERIFY(same_elements(document_elements(*document_), moved));
  }

  // Through fixture 08's dual block (slots 11-13), Down and Right follow
  // document order: left speech, then the right cue and speech. A selection
  // made that way copies document-order text and highlights both columns.
  void caret_and_selection_through_dual_dialogue() {
    QVERIFY(open(QStringLiteral("08-dual-dialogue-and-contd")));
    const int left = position_of(QStringLiteral("Then hold the ladder steady and stop"));
    const int left_end = position_of(QStringLiteral("arguing with me.")) + 16;
    const int right_cue = position_of(QStringLiteral("MARA\nI am holding"));
    const int right = position_of(QStringLiteral("I am holding it."));
    editor_->setProperty("cursorPosition", left);
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 12)) && near(caret_rect(cursor()).x(), 108));
    press(Qt::Key_Down);
    QCOMPARE(cursor(), position_of(QStringLiteral("and stop arguing")));
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 13)));
    press(Qt::Key_Down);
    QCOMPARE(cursor(), right_cue);
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 11)) && near(caret_rect(cursor()).x(), 403.2));
    press(Qt::Key_Down);
    QCOMPARE(cursor(), right);
    QVERIFY(near(caret_baseline(cursor()), baseline(1, 12)) && near(caret_rect(cursor()).x(), 331.2));
    editor_->setProperty("cursorPosition", left_end);
    press(Qt::Key_Right);
    QCOMPARE(cursor(), right_cue);
    press(Qt::Key_Left);
    QCOMPARE(cursor(), left_end);

    editor_->setProperty("cursorPosition", left);
    for (int i = 0; i < 3; ++i) press(Qt::Key_Down, Qt::ShiftModifier);
    QCOMPARE(editor_->property("selectedText").toString(),
             QStringLiteral("Then hold the ladder steady and stop arguing with me.\nMARA\n"));
    press(Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("Then hold the ladder steady and stop arguing with me.\nMARA"));
    editor_->setProperty("cursorVisible", false);
    const auto image = view_->grabWindow();
    image.save(QStringLiteral(APOLLO_ARTIFACT_DIR "/dual-selection.png"));
    auto highlighted = [&](double x0, double x1, int slot) {
      // Include the upper selection background. At the presentation scale,
      // native subpixel glyphs fill most of the five points above a baseline;
      // counting exact background pixels there misses a correctly selected cue.
      // Eight points stays clear of the preceding line's selection rectangle.
      return this->highlighted(image, QRectF(x0 * px_per_point, (baseline(1, slot) - 8) * px_per_point,
                                            (x1 - x0) * px_per_point, 8 * px_per_point));
    };
    QVERIFY(highlighted(108, 300, 12) > 100);
    QVERIFY(highlighted(108, 280, 13) > 100);
    QVERIFY(highlighted(403.2, 403.2 + 4 * profile::advance, 11) > 20);
    QCOMPARE(highlighted(331.2, 540, 12), 0);
  }

  // Generated continuation text keeps the source cue's emphasis, like the
  // PDF; the automatic suffix is plain.
  void continuation_text_keeps_the_cue_style() {
    QVERIFY(open(QStringLiteral("02-dialogue-split")));
    const int cue = position_of(QStringLiteral("MARA (V.O.)"));
    QTextCursor bold(document_.get());
    bold.setPosition(cue);
    bold.setPosition(cue + 11, QTextCursor::KeepAnchor);
    QTextCharFormat format;
    format.setFontWeight(QFont::Bold);
    bold.mergeCharFormat(format);
    QVERIFY(layout()->error().isEmpty());
    const auto& runs = static_cast<DecorationModel*>(layout()->decoration_model())->runs();
    auto run = [&](const QString& text) {
      return std::find_if(runs.begin(), runs.end(), [&](const DecorationModel::Run& r) { return r.text == text; });
    };
    const auto name = run(QStringLiteral("MARA (V.O.)")), suffix = run(QStringLiteral(" (CONT’D)"));
    QVERIFY(name != runs.end() && suffix != runs.end());
    QVERIFY(name->bold && !suffix->bold);
    QVERIFY(near(name->x * pt, 288) && near(name->baseline * pt, baseline(2, 1)));
    QVERIFY(near(suffix->x * pt, 288 + 11 * profile::advance));
    int drawn_bold = 0;
    std::vector<QQuickItem*> items{view_->rootObject()};
    while (!items.empty()) {
      auto* item = items.back();
      items.pop_back();
      if (item->property("text").toString() == QStringLiteral("MARA (V.O.)"))
        drawn_bold += item->property("font").value<QFont>().bold() ? 1 : -1;
      for (auto* child : item->childItems()) items.push_back(child);
    }
    QCOMPARE(drawn_bold, 1);
    QCOMPARE(ink_mismatch(QStringLiteral("styled-continuation")), std::make_pair(0, 0));
  }

  // A typed (CONT'D) where G1 generates one is shown once, as the generated
  // form; the typed suffix stays document text, drawn transparent beneath.
  // Typed in lowercase it is the same suffix: the cue is presented in capitals.
  void typed_continuation_is_shown_once_data() {
    QTest::addColumn<QString>("suffix");
    QTest::newRow("capitals") << QStringLiteral(" (CONT'D)");
    QTest::newRow("lowercase") << QStringLiteral(" (cont'd)");
  }
  void typed_continuation_is_shown_once() {
    QFETCH(QString, suffix);
    QVERIFY(open(QStringLiteral("08-dual-dialogue-and-contd")));
    const int cue = position_of(QStringLiteral("MARA\nHigher."));
    editor_->setProperty("cursorPosition", cue + 4);
    type(suffix);
    QVERIFY(document_->toPlainText().contains(QStringLiteral("MARA") + suffix + QStringLiteral("\nHigher.")));
    const auto shown = shown_lines(*document_);
    const auto line = std::find_if(shown.begin(), shown.end(), [](const ShownLine& l) { return l.page == 1 && l.slot == 8; });
    QVERIFY(line != shown.end());
    QCOMPARE(line->text, QStringLiteral("MARA (CONT’D)"));
    QVERIFY(near(line->x, 288));
    QVERIFY(draws(block_at(cue), QStringLiteral("MARA")));
    bool hidden = false;
    for (const auto& range : block_at(cue).layout()->formats())
      hidden |= range.start == 4 && range.length == 9 && range.format.foreground().color().alpha() == 0;
    QVERIFY(hidden);
    QCOMPARE(ink_mismatch(QStringLiteral("typed-continuation-") + QTest::currentDataTag()), std::make_pair(0, 0));
    // When the engine rejects the document (centered text longer than a page)
    // the unpaginated fallback has no generated text, so nothing stays hidden.
    QTextCursor cursor(document_.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    insert_elements(cursor, {{ElementType::centered, std::string(60 * 57, 'x')}});
    QVERIFY(!layout()->error().isEmpty());
    for (const auto& range : block_at(cue).layout()->formats())
      QVERIFY(range.format.foreground().color().alpha() != 0);
  }

  // The title page is the first page card; a manual break's caret line sits
  // two slots after the last line of the page it ends (page 1, slot 5).
  void title_page_and_manual_break_carets() {
    QVERIFY(open(QStringLiteral("07-title-page-and-manual-break")));
    const int title = position_of(QStringLiteral("The Lamp Room"));
    QVERIFY(draws(block_at(title), QStringLiteral("THE LAMP ROOM")));
    QVERIFY(near(caret_baseline(title), baseline(0, 4)) && near(caret_rect(title).x(), 306 - 13 * profile::advance / 2));
    auto block = document_->begin();
    while (element_type(block) != ElementType::page_break) block = block.next();
    QVERIFY(near(caret_baseline(block.position()), baseline(1, 5)) && near(caret_rect(block.position()).x(), 108));
    QCOMPARE(ink_mismatch(QStringLiteral("title-page")), std::make_pair(0, 0));
  }

  // TextEdit draws only the visible part of a large document; scrolling
  // must still draw every line that comes into view.
  void scrolled_pages_render() {
    std::vector<Element> elements;
    for (int i = 0; i < 250; ++i) elements.push_back({ElementType::action, "Filler sentence " + std::to_string(i) + " keeps the page full."});
    QVERIFY(open_elements(elements));
    QVERIFY(document_->characterCount() > 10000);
    view_->rootObject()->findChild<QQuickItem*>("flick")->setProperty("contentY", 4 * page_stride + 100);
    QCOMPARE(ink_mismatch(QStringLiteral("scrolled")), std::make_pair(0, 0));
  }

  // After each kind of edit, the incrementally updated layout equals a fresh
  // layout of the same elements. A keystroke that does not repaginate
  // rebuilds only its own block's lines.
  void incremental_layout_matches_fresh_layout() {
    QVERIFY(open(QStringLiteral("06-action-split")));
    auto matches_fresh = [&] {
      const auto fresh = screenplay_document(document_elements(*document_), font_);
      const auto a = laid_out_lines(*document_), b = laid_out_lines(*fresh);
      const auto& da = layout()->decoration_list();
      const auto& db = static_cast<ScreenplayLayout*>(fresh->documentLayout())->decoration_list();
      bool same = a.size() == b.size() && da.size() == db.size();
      for (std::size_t i = 0; same && i < a.size(); ++i)
        same = a[i].text == b[i].text && near(a[i].x, b[i].x) && near(a[i].baseline, b[i].baseline);
      for (std::size_t i = 0; same && i < da.size(); ++i) same = da[i].text == db[i].text && da[i].baseline == db[i].baseline;
      return same && layout()->error().isEmpty();
    };
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("Filler line 01.")) + 6);
    type(QStringLiteral("x"));
    QCOMPARE(layout()->rebuilt_blocks(), 1);
    QVERIFY(matches_fresh());
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("Somewhere")));
    type(QStringLiteral("Lightning. "));
    QVERIFY(matches_fresh());
    press(Qt::Key_Return);
    QVERIFY(matches_fresh());
    press(Qt::Key_Backspace);
    QVERIFY(matches_fresh());
    editor_->setProperty("cursorPosition", position_of(QStringLiteral("Filler line 03.")));
    press(Qt::Key_Backtab, Qt::ShiftModifier);
    QVERIFY(matches_fresh());
    QGuiApplication::clipboard()->setText(QStringLiteral("A pasted line.\nAnother pasted line."));
    press(Qt::Key_V, Qt::ControlModifier);
    QVERIFY(matches_fresh());
    for (int i = 0; i < 4; ++i) { press(Qt::Key_Z, Qt::ControlModifier); QVERIFY(matches_fresh()); }
  }
};

QTEST_MAIN(EditorTests)
#include "editor_tests.moc"
