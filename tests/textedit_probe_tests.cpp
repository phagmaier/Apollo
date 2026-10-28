#include "editor/probe_view.h"
#include "editor/screenplay_layout.h"
#include "fountain/import.h"

#include <QClipboard>
#include <QDir>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTextBlock>
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

// Independent profile values (docs/export-profile.md) in points.
// Slot 0 is the page-number line: top at 0.5 in, so its baseline is 9 pt
// lower as for body slot 1 (top 1 in, baseline 81 pt); right edge 7.25 in.
double baseline(int page, int slot) {
  return (page - 1) * page_stride_pt + (slot ? 81 + (slot - 1) * profile::line_pitch : 45);
}
double profile_x(const Expected& line) {
  const double width = line.text.toUcs4().size() * profile::advance;
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
struct Shown { QString text; double x, baseline, top, height, width; };
std::vector<Shown> shown_lines(const QTextDocument& document) {
  std::vector<Shown> shown;
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
}

class TextEditProbe : public QObject {
  Q_OBJECT
  QFont font_;
  std::unique_ptr<QTextDocument> document_;
  std::unique_ptr<QQuickView> view_;
  QQuickItem* editor_ = nullptr;

  ScreenplayLayout* layout() const { return static_cast<ScreenplayLayout*>(document_->documentLayout()); }
  bool open(const QString& name) {
    view_.reset();
    document_ = screenplay_document(import_fountain(read(fixture_path(name, ".fountain"))), font_);
    view_ = std::make_unique<QQuickView>();
    editor_ = show_probe(*view_, *document_);
    view_->resize(static_cast<int>(page_width), static_cast<int>(2 * page_stride));
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
  int cursor() const { return editor_->property("cursorPosition").toInt(); }
  double caret_baseline(int position) const { return caret_rect(position).top() + QFontMetricsF(font_).ascent() * pt; }
  // Pixels darker than the page or background that are outside every
  // expected line, and lines with no ink, in a grab of the current view.
  std::pair<int, int> ink_mismatch(const QString& artifact) {
    editor_->setProperty("cursorVisible", false);
    const auto image = view_->grabWindow();
    image.save(QStringLiteral(APOLLO_ARTIFACT_DIR "/") + artifact + ".png");
    std::vector<QRectF> regions;
    for (const auto& line : shown_lines(*document_))
      if (!line.text.isEmpty())
        regions.push_back(QRectF(line.x * px_per_point, line.top * px_per_point, line.width * px_per_point,
                                 line.height * px_per_point).adjusted(-1, -1, 1, 1));
    const QFontMetricsF metrics(font_);
    for (const auto& decoration : layout()->decoration_list())
      regions.push_back(QRectF(decoration.baseline.x(), decoration.baseline.y() - metrics.ascent(),
                               metrics.horizontalAdvance(decoration.text), metrics.height()).adjusted(-1, -1, 1, 1));
    std::vector<bool> inked(regions.size());
    int stray = 0;
    for (int y = 0; y < image.height(); ++y)
      for (int x = 0; x < image.width(); ++x) {
        if (qGray(image.pixel(x, y)) >= 96) continue;
        const QPointF point(x + 0.5, y + 0.5);
        bool inside = false;
        for (std::size_t i = 0; i < regions.size(); ++i)
          if (regions[i].contains(point)) { inked[i] = true; inside = true; }
        stray += !inside;
      }
    return {stray, static_cast<int>(std::count(inked.begin(), inked.end(), false))};
  }

private slots:
  void initTestCase() {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    font_ = courier_prime(QStringLiteral(APOLLO_FONT_DIR));
    QVERIFY(QDir().mkpath(QStringLiteral(APOLLO_ARTIFACT_DIR)));
  }
  // Any warning, such as a QML binding error, fails the test.
  void init() { QTest::failOnWarning(QRegularExpression(QStringLiteral(".*"))); }
  void cleanup() { view_.reset(); document_.reset(); }

  // The document keeps each element's type, text, dual flag, and emphasis.
  void document_round_trips_elements() {
    for (const char* name : {"01-element-geometry", "02-dialogue-split", "03-heading-keep-with-next", "04-dialogue-push",
                             "05-parenthetical-at-break", "06-action-split", "07-title-page-and-manual-break",
                             "08-dual-dialogue-and-contd"}) {
      const auto elements = import_fountain(read(fixture_path(name, ".fountain")));
      const auto document = screenplay_document(elements, font_);
      const auto loaded = document_elements(*document);
      QCOMPARE(loaded.size(), elements.size());
      auto styles_per_byte = [](const Element& element) {
        std::vector<int> flags(element.text.size());
        for (const auto& span : element.styles)
          for (auto i = span.start; i < span.end; ++i) flags[i] |= 1 << static_cast<int>(span.style);
        return flags;
      };
      for (std::size_t i = 0; i < elements.size(); ++i) {
        QVERIFY2(loaded[i].type == elements[i].type && loaded[i].text == elements[i].text &&
                 loaded[i].dual_right == elements[i].dual_right &&
                 styles_per_byte(loaded[i]) == styles_per_byte(elements[i]), name);
      }
    }
  }

  // Each fixture line appears at its hand-derived page, slot, and profile
  // column. Generated text is a decoration and never document text.
  void lines_follow_fixture_pages_data() {
    QTest::addColumn<QString>("name");
    for (const char* name : {"01-element-geometry", "02-dialogue-split", "03-heading-keep-with-next", "04-dialogue-push",
                             "05-parenthetical-at-break", "06-action-split", "08-dual-dialogue-and-contd"})
      QTest::newRow(name) << name;
  }
  void lines_follow_fixture_pages() {
    QFETCH(QString, name);
    QVERIFY(open(name));
    auto shown = shown_lines(*document_);
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
        if (shown[i].text == want.text ||
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
    auto highlighted = [&](double top, double bottom) {
      int count = 0;
      for (int y = static_cast<int>(top * px_per_point); y < static_cast<int>(bottom * px_per_point); ++y)
        for (int x = 144; x < 144 + 200; ++x) count += image.pixelColor(x, y) == QColor("#8fb4e8");
      return count;
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

  // A new cue has no dialogue yet, which the engine rejects. Every block
  // must still have a caret line so typing can continue, and the paginated
  // layout returns once the speech is complete.
  void engine_rejection_keeps_document_editable() {
    QVERIFY(open(QStringLiteral("02-dialogue-split")));
    QTextCursor cursor(document_.get());
    cursor.movePosition(QTextCursor::End);
    QTextBlockFormat cue;
    cue.setProperty(element_type_property, static_cast<int>(ElementType::cue));
    cursor.insertBlock(cue);
    cursor.insertText(QStringLiteral("THEO"));
    QCOMPARE(layout()->error(), QStringLiteral("cue without speech is not supported"));
    for (auto block = document_->begin(); block != document_->end(); block = block.next())
      QVERIFY(block.layout()->lineCount() > 0 && layout()->blockBoundingRect(block).isValid());
    editor_->setProperty("cursorPosition", document_->characterCount() - 1);
    type(QStringLiteral("S"));
    QVERIFY(document_->toPlainText().endsWith(QStringLiteral("THEOS")));
    QTextBlockFormat dialogue;
    dialogue.setProperty(element_type_property, static_cast<int>(ElementType::dialogue));
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock(dialogue);
    cursor.insertText(QStringLiteral("Nobody."));
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
};

QTEST_MAIN(TextEditProbe)
#include "textedit_probe_tests.moc"
