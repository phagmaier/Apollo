#include "editor/screenplay_layout.h"
#include "source/screenplay_format.h"

#include <QGuiApplication>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <algorithm>
#include <cmath>

using namespace apollo;
using namespace apollo::editor;

namespace {
bool near(double actual, double expected) { return std::abs(actual - expected) < 0.01; }

std::vector<Element> identified(std::vector<Element> elements) {
  for (std::size_t i = 0; i < elements.size(); ++i) elements[i].id = "element-" + std::to_string(i);
  return elements;
}

struct ExpectedLine {
  int page, slot;
  ElementType type;
  const char* text;
  std::size_t source, bytes;
  double x, baseline, width;
  char column = 0;
};
}

class PageBreakLayoutTests : public QObject {
  Q_OBJECT
private:
  QFont font_;

  void check_pages(const DocumentLayout& laid, bool titled, int body_pages) {
    QCOMPARE(laid.pages.size(), static_cast<std::size_t>(body_pages + (titled ? 1 : 0)));
    if (titled) {
      QCOMPARE(laid.pages[0].body_number, 0);
      QVERIFY(!laid.pages[0].number);
    }
    for (int body = 1; body <= body_pages; ++body) {
      const auto& page = laid.pages[body - (titled ? 0 : 1)];
      QCOMPARE(page.body_number, body);
      if (body == 1) { QVERIFY(!page.number); continue; }
      QVERIFY(page.number.has_value());
      QCOMPARE(page.number->text, std::to_string(body) + ".");
      // These small examples use two-glyph numbers, right aligned at 522 pt.
      QVERIFY(near(page.number->geometry.x, 507.609375));
      QVERIFY(near(page.number->geometry.baseline, 45));
      QVERIFY(near(page.number->geometry.width, 14.390625));
    }
  }

  void check_line(const Line& line, const ExpectedLine& expected) {
    QCOMPARE(line.page, expected.page);
    QCOMPARE(line.slot, expected.slot);
    QVERIFY(line.type == expected.type);
    QCOMPARE(line.text, std::string(expected.text));
    QCOMPARE(line.source, expected.source);
    QCOMPARE(line.source_offset, std::size_t(0));
    QCOMPARE(line.source_length, expected.bytes);
    QCOMPARE(line.replaced_length, std::size_t(0));
    QCOMPARE(line.column, expected.column);
    QVERIFY(near(line.geometry.x, expected.x));
    QVERIFY(near(line.geometry.baseline, expected.baseline));
    QVERIFY(near(line.geometry.width, expected.width));
  }

  void check_markers(QTextDocument& document, const std::vector<double>& expected_y, bool has_text = false) {
    auto* layout = static_cast<ScreenplayLayout*>(document.documentLayout());
    const auto rows = layout->pageBreaks();
    QCOMPARE(rows.size(), static_cast<qsizetype>(expected_y.size()));
    std::size_t marker = 0;
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
      if (element_type(block) != ElementType::page_break) continue;
      QVERIFY(marker < expected_y.size());
      const auto row = rows[static_cast<qsizetype>(marker)].toMap();
      QCOMPARE(row.value(QStringLiteral("position")).toInt(), block.position());
      QVERIFY(near(row.value(QStringLiteral("y")).toReal(), expected_y[marker]));
      QCOMPARE(row.value(QStringLiteral("hasText")).toBool(), has_text);
      ++marker;
    }
  }

private slots:
  void initTestCase() { font_ = courier_prime(QStringLiteral(APOLLO_FONT_DIR)); }

  void leading_consecutive_trailing_data() {
    QTest::addColumn<bool>("titled");
    QTest::newRow("body-only") << false;
    QTest::newRow("title-and-body") << true;
  }

  void leading_consecutive_trailing() {
    QFETCH(bool, titled);
    std::vector<Element> elements = {
        {ElementType::page_break, ""},
        {ElementType::action, "First.", false, {{0, 6, Style::bold}}},
        {ElementType::page_break, ""}, {ElementType::page_break, ""},
        {ElementType::action, "Last."}, {ElementType::page_break, ""}};
    if (titled) elements.insert(elements.begin(), {ElementType::title, "Beacon"});
    elements = identified(std::move(elements));
    const auto laid = layout_document(elements);
    check_pages(laid, titled, 5);
    QCOMPARE(laid.lines.size(), std::size_t(titled ? 3 : 2));
    if (titled)
      check_line(laid.lines[0], {0, 4, ElementType::title, "BEACON", 0, 6, 284.4140625, 117, 43.171875});
    const std::size_t shift = titled ? 1 : 0;
    check_line(laid.lines[shift], {2, 1, ElementType::action, "First.", 1 + shift, 6, 108, 81, 43.171875});
    check_line(laid.lines[shift + 1], {4, 1, ElementType::action, "Last.", 4 + shift, 5, 108, 81, 35.9765625});
    QVERIFY(laid.lines[shift].styles == std::vector<StyleSpan>({{0, 6, Style::bold}}));

    auto document = screenplay_document(elements, font_);
    auto* layout = static_cast<ScreenplayLayout*>(document->documentLayout());
    QVERIFY2(layout->error().isEmpty(), qPrintable(layout->error()));
    QCOMPARE(layout->pageCount(), titled ? 6 : 5);
    // 792 pt pages at 96 dpi, 32 px gaps, labels 4 px below each page.
    check_markers(*document, titled ? std::vector<double>{2148, 3236, 4324, 5412}
                                    : std::vector<double>{1060, 2148, 3236, 4324});
    const auto shown = shown_lines(*document);
    QCOMPARE(shown.size(), std::size_t(titled ? 7 : 6)); // Authored lines plus numbers 2 through 5.
    QVERIFY(std::none_of(shown.begin(), shown.end(), [](const ShownLine& line) { return line.kind == QStringLiteral("page-break"); }));
    QVERIFY(document_elements(*document) == elements);

    // A text edit rebuilds marker positions without changing the page gaps.
    QTextCursor cursor(document.get());
    auto action = document->findBlockByNumber(static_cast<int>(shift + 1));
    cursor.setPosition(action.position());
    cursor.insertText(QStringLiteral("A"));
    const auto rows = layout->pageBreaks();
    QCOMPARE(rows[1].toMap().value(QStringLiteral("position")).toInt(), document->findBlockByNumber(static_cast<int>(shift + 2)).position());
    check_markers(*document, titled ? std::vector<double>{2148, 3236, 4324, 5412}
                                    : std::vector<double>{1060, 2148, 3236, 4324});
  }

  void only_markers_data() { leading_consecutive_trailing_data(); }

  void only_markers() {
    QFETCH(bool, titled);
    std::vector<Element> elements = {{ElementType::page_break, ""}, {ElementType::page_break, ""}};
    if (titled) elements.insert(elements.begin(), {ElementType::title, "Beacon"});
    const auto laid = layout_document(elements);
    check_pages(laid, titled, 3);
    QCOMPARE(laid.lines.size(), std::size_t(titled ? 1 : 0));
    auto document = screenplay_document(elements, font_);
    auto* layout = static_cast<ScreenplayLayout*>(document->documentLayout());
    QVERIFY(layout->error().isEmpty());
    QCOMPARE(layout->pageCount(), titled ? 4 : 3);
    check_markers(*document, titled ? std::vector<double>{2148, 3236} : std::vector<double>{1060, 2148});
    QCOMPARE(shown_lines(*document).size(), std::size_t(titled ? 3 : 2));
  }

  void marker_after_full_page() {
    std::string full_page = "Line.";
    for (int i = 1; i < 56; ++i) full_page += "\nLine.";
    const auto elements = identified({{ElementType::action, full_page},
        {ElementType::page_break, ""}, {ElementType::action, "Next."}});
    const auto laid = layout_document(elements);
    check_pages(laid, false, 2);
    QCOMPARE(laid.lines.size(), std::size_t(57));
    QCOMPARE(laid.lines[55].page, 1);
    QCOMPARE(laid.lines[55].slot, 56);
    QVERIFY(near(laid.lines[55].geometry.baseline, 741));
    QCOMPARE(laid.lines[55].source_offset, std::size_t(330));
    check_line(laid.lines[56], {2, 1, ElementType::action, "Next.", 2, 5, 108, 81, 35.9765625});
    auto document = screenplay_document(elements, font_);
    auto* layout = static_cast<ScreenplayLayout*>(document->documentLayout());
    QVERIFY2(layout->error().isEmpty(), qPrintable(layout->error()));
    check_markers(*document, {1060});
    QCOMPARE(shown_lines(*document).size(), std::size_t(58));
  }

  void speech_and_dual_adjacent_markers() {
    const auto elements = identified({
        {ElementType::page_break, ""}, {ElementType::cue, "MARA"},
        {ElementType::parenthetical, "(softly)"},
        {ElementType::dialogue, "Café.", false, {{0, 6, Style::italic}}},
        {ElementType::page_break, ""},
        {ElementType::cue, "LEFT"}, {ElementType::dialogue, "One."},
        {ElementType::cue, "RIGHT", true}, {ElementType::dialogue, "Two."},
        {ElementType::page_break, ""}});
    const auto laid = layout_document(elements);
    check_pages(laid, false, 4);
    const std::vector<ExpectedLine> expected = {
        {2, 1, ElementType::cue, "MARA", 1, 4, 288, 81, 28.78125},
        {2, 2, ElementType::parenthetical, "(softly)", 2, 8, 244.8, 93, 57.5625},
        {2, 3, ElementType::dialogue, "Café.", 3, 6, 216, 105, 35.9765625},
        {3, 1, ElementType::cue, "LEFT", 5, 4, 180, 81, 28.78125, 'L'},
        {3, 1, ElementType::cue, "RIGHT", 7, 5, 403.2, 81, 35.9765625, 'R'},
        {3, 2, ElementType::dialogue, "One.", 6, 4, 108, 93, 28.78125, 'L'},
        {3, 2, ElementType::dialogue, "Two.", 8, 4, 331.2, 93, 28.78125, 'R'}};
    QCOMPARE(laid.lines.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) check_line(laid.lines[i], expected[i]);
    QVERIFY(laid.lines[2].styles == std::vector<StyleSpan>({{0, 6, Style::italic}}));
    auto document = screenplay_document(elements, font_);
    auto* layout = static_cast<ScreenplayLayout*>(document->documentLayout());
    QVERIFY2(layout->error().isEmpty(), qPrintable(layout->error()));
    check_markers(*document, {1060, 2148, 3236});
    QVERIFY(document_elements(*document) == elements);
    const auto shown = shown_lines(*document);
    QCOMPARE(shown.size(), std::size_t(10)); // Seven speech lines plus three page numbers.
    QCOMPARE(std::count_if(shown.begin(), shown.end(), [](const ShownLine& line) { return line.kind.startsWith(QStringLiteral("dual-")); }), 4);
  }

  void legacy_marker_text_is_retained_and_refused() {
    const auto elements = identified({{ElementType::action, "Before."},
        {ElementType::page_break, "Legacy café.", false, {{0, 6, Style::bold}}},
        {ElementType::action, "After."}});
    bool refused = false;
    try { (void)layout_document(elements); }
    catch (const std::runtime_error& error) {
      refused = true;
      QVERIFY(QString::fromUtf8(error.what()).contains(QStringLiteral("page break contains text")));
    }
    QVERIFY(refused);
    const source::Screenplay snapshot{7, elements};
    QVERIFY(source::parse_screenplay(source::serialize_screenplay(snapshot)) == snapshot);
    auto document = screenplay_document(elements, font_);
    auto* layout = static_cast<ScreenplayLayout*>(document->documentLayout());
    QVERIFY(layout->error().contains(QStringLiteral("page break contains text")));
    check_markers(*document, {1060}, true);
    QVERIFY(document_elements(*document) == elements);
    const auto marker = document->findBlockByNumber(1);
    const auto shown = shown_lines(*document);
    QVERIFY(std::any_of(shown.begin(), shown.end(), [](const ShownLine& line) { return line.text == QStringLiteral("Legacy café."); }));
    QVERIFY(layout->decoration_list().empty());

    // Once explicitly cleared, the normal paginated layout replaces fallback
    // rows and removes the warning; undo restores the legacy source untouched.
    QTextCursor cursor(marker);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    QVERIFY(layout->error().isEmpty());
    check_markers(*document, {1060});
    document->undo();
    QVERIFY(layout->error().contains(QStringLiteral("page break contains text")));
    check_markers(*document, {1060}, true);
    QVERIFY(document_elements(*document) == elements);
  }
};

QTEST_MAIN(PageBreakLayoutTests)
#include "page_break_layout_tests.moc"
