#include "editor/find_replace.h"
#include "editor/screenplay_layout.h"
#include "source/screenplay_format.h"

#include <QSignalSpy>
#include <QtTest>

using namespace apollo;
using namespace apollo::editor;

class FindReplaceTests : public QObject {
  Q_OBJECT
private slots:
  void literal_case_words_and_navigation() {
    auto doc = screenplay_document({{ElementType::action, "Cat cat scatter cat. a.b axb"}}, QFont("monospace", 12));
    FindReplace find(doc.get());
    QSignalSpy selections(&find, &FindReplace::selectionRequested);
    find.begin(4, 4, 4);
    find.configure("cat", false, false, false);
    QTRY_VERIFY(!find.busy());
    QCOMPARE(find.count(), 4);
    QCOMPARE(selections.last(), QVariantList({4, 7}));
    find.next(true); QCOMPARE(selections.last(), QVariantList({0, 3}));
    find.next(true); QCOMPARE(selections.last(), QVariantList({16, 19}));
    find.next(); QCOMPARE(selections.last(), QVariantList({0, 3}));
    find.configure("cat", true, true, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 2);
    find.configure("a.b", false, false, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 1);
    find.configure("", false, false, false);
    QVERIFY(!find.busy()); QCOMPARE(find.count(), 0);
    QCOMPARE(find.replace("x", true), 0);
  }

  void selection_scope_tracks_replacements_and_undo() {
    auto doc = screenplay_document({{ElementType::action, "cat cat cat cat"}}, QFont("monospace", 12));
    const auto before = document_elements(*doc);
    FindReplace find(doc.get());
    QSignalSpy selections(&find, &FindReplace::selectionRequested);
    find.begin(4, 11, 4); // Reversed opening selection, two complete matches.
    find.configure("cat", true, true, true);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 2);
    QCOMPARE(find.replace("elephant"), 1);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 1);
    QCOMPARE(doc->toPlainText(), QString("cat elephant cat cat"));
    QCOMPARE(selections.last(), QVariantList({13, 16}));
    QCOMPARE(find.replace("ox", true), 1);
    QCOMPARE(doc->toPlainText(), QString("cat elephant ox cat"));
    doc->undo();
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 1);
    doc->undo();
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 2);
    QCOMPARE(document_elements(*doc), before);
    find.configure("at", false, false, true);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 2);
    // A selection edge cannot turn part of a word into a whole word.
    find.begin(5, 5, 7); find.configure("at", false, true, true);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 0);
    // Deleting the entire captured range must still allow returning to whole
    // script scope; the scope checkbox must not become disabled while checked.
    find.begin(0, 0, 3); find.configure("cat", true, true, true);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 1);
    QCOMPARE(find.replace("", true), 1);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 0);
    QVERIFY(find.hasScope()); QCOMPARE(find.status(), QString("No matches"));
    find.configure("cat", true, true, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 3);
    doc->undo(); QCOMPARE(document_elements(*doc), before);
  }

  void replace_all_preserves_structure_emphasis_and_reopens() {
    auto doc = screenplay_document({{ElementType::title, "CAT"},
        {ElementType::cue, "cat", false, {{0, 3, Style::bold}}},
        {ElementType::dialogue, "😀 cat\ncat end", false, {{5, 8, Style::italic}, {13, 16, Style::underline}}},
        {ElementType::cue, "JANE", true}, {ElementType::dialogue, "cat"},
        {ElementType::page_break, ""}, {ElementType::action, "cat tail", false, {{4, 8, Style::bold}}}},
        QFont("monospace", 12));
    const auto before = document_elements(*doc);
    auto expected = before;
    expected[0].text = "DOG"; // Case-sensitive search below changes only this field.
    FindReplace find(doc.get());
    find.begin(0, 0, 0); find.configure("CAT", true, false, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 1);
    QCOMPARE(find.replace("DOG"), 1);
    QCOMPARE(document_elements(*doc), expected);
    doc->undo();
    find.configure("cat", false, true, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 6);
    expected = before;
    expected[0].text = "ox"; expected[1].text = "ox"; expected[1].styles = {{0, 2, Style::bold}};
    expected[2].text = "😀 ox\nox end"; expected[2].styles = {{5, 7, Style::italic}, {11, 14, Style::underline}};
    expected[4].text = "ox"; expected[6].text = "ox tail"; expected[6].styles = {{3, 7, Style::bold}};
    QCOMPARE(find.replace("ox", true), 6);
    QCOMPARE(document_elements(*doc), expected);
    QCOMPARE(source::parse_screenplay(source::serialize_screenplay({0, document_elements(*doc)})).elements, expected);
    doc->undo(); QCOMPARE(document_elements(*doc), before);
    doc->redo(); QCOMPARE(document_elements(*doc), expected);
  }

  void unicode_words_and_authored_boundaries() {
    auto doc = screenplay_document({{ElementType::heading, "int. room - day"},
        {ElementType::action, "😀 cat café café cat_ cat2 cat's\ncat"},
        {ElementType::page_break, ""}, {ElementType::action, "cat"}}, QFont("monospace", 12));
    FindReplace find(doc.get()); QSignalSpy selections(&find, &FindReplace::selectionRequested);
    find.begin(0, 0, 0); find.configure("cat", false, true, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 4);
    QCOMPARE(selections.first()[0].toInt(), doc->findBlockByNumber(1).position() + 3);
    find.configure("cafe", false, true, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 0); // Combining mark belongs to the word.
    find.configure("INT.", true, false, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 0); // Presented capitals are not authored text.
    find.configure("INT.", false, false, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 1);
    find.configure("cat\ncat", false, false, false);
    QVERIFY(!find.busy()); QCOMPARE(find.count(), 0);
    QVERIFY(find.status().contains("one line"));
  }

  void deleting_matches_keeps_empty_elements_and_skips_marker_text() {
    auto doc = screenplay_document({{ElementType::title, "cat"}, {ElementType::cue, "cat"},
        {ElementType::dialogue, "cat\ncat"}, {ElementType::cue, "JANE", true},
        {ElementType::dialogue, "cat"}, {ElementType::page_break, "cat"}}, QFont("monospace", 12));
    const auto before = document_elements(*doc);
    auto expected = before;
    expected[0].text.clear(); expected[1].text.clear();
    expected[2].text = "\n"; expected[4].text.clear();
    FindReplace find(doc.get());
    find.begin(0, 0, 0); find.configure("cat", true, true, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 5);
    QCOMPARE(find.replace("", true), 5);
    QCOMPARE(document_elements(*doc), expected);
    QCOMPARE(source::parse_screenplay(source::serialize_screenplay({0, expected})).elements, expected);
    doc->undo(); QCOMPARE(document_elements(*doc), before);
  }

  void stale_cancelled_results_and_invalid_or_noop_replacement() {
    auto doc = screenplay_document({{ElementType::action, "cat cat"}}, QFont("monospace", 12));
    FindReplace find(doc.get());
    find.begin(0, 0, 0); find.configure("cat", true, true, false);
    QVERIFY(find.busy()); QCOMPARE(find.replace("dog", true), 0);
    QTRY_VERIFY(!find.busy());
    QCOMPARE(find.replace("cat", true), 0); QVERIFY(!doc->isUndoAvailable());
    QTRY_VERIFY(!find.busy());
    for (const auto& invalid : {QString("a\nb"), QString("a\tb"), QString(QChar::ParagraphSeparator), QString(4097, 'x')}) {
      QCOMPARE(find.replace(invalid, true), 0);
      QCOMPARE(doc->toPlainText(), QString("cat cat"));
      QVERIFY(!doc->isUndoAvailable());
    }
    QTextCursor cursor(doc.get()); cursor.select(QTextCursor::Document); cursor.insertText("dog");
    QCOMPARE(find.replace("bad", true), 0);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 0);
    find.configure("dog", false, false, false); find.dismiss();
    QTest::qWait(10); QCOMPARE(find.count(), 0); QVERIFY(!find.busy());
    QCOMPARE(find.replace("bad"), 0);
    // Search destruction cancels queued batches with no document access.
    auto transient = std::make_unique<FindReplace>(doc.get());
    transient->begin(0, 0, 0); transient->configure("dog", false, false, false); transient.reset();
    QTest::qWait(10); QCOMPARE(doc->toPlainText(), QString("dog"));
  }

  void bounded_batches_and_match_limit() {
    std::vector<Element> elements(300, {ElementType::action, "cat"});
    auto doc = screenplay_document(elements, QFont("monospace", 12));
    FindReplace find(doc.get());
    find.begin(0, 0, 0); find.configure("cat", false, false, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 300);
    find.configure("missing", false, false, false);
    find.configure("cat", false, false, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 300);
    QTextCursor cursor(doc.get()); cursor.select(QTextCursor::Document);
    cursor.insertText(QString(8190, 'x') + "cat" + QString(8190, 'x') + "cat");
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 2); // Match straddles a scan chunk.
    cursor.select(QTextCursor::Document); cursor.insertText(QString(100001, 'a'));
    find.configure("a", true, false, false);
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 0);
    QVERIFY(find.status().contains("100,000"));
    QCOMPARE(find.replace("b", true), 0);
    QCOMPARE(doc->toPlainText(), QString(100001, 'a'));
    cursor.select(QTextCursor::Document); cursor.insertText(QString(1000, 'a'));
    QTRY_VERIFY(!find.busy()); QCOMPARE(find.count(), 1000);
    QCOMPARE(find.replace(QString(4096, 'x'), true), 0);
    QVERIFY(find.status().contains("1,000,000"));
    QCOMPARE(doc->toPlainText(), QString(1000, 'a'));
  }
};
QTEST_MAIN(FindReplaceTests)
#include "find_replace_tests.moc"
