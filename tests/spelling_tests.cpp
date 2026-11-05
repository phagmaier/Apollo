#include "editor/spelling.h"
#include "editor/screenplay_layout.h"
#include <QFile>
#include <QLockFile>
#include "source/screenplay_format.h"
#include <QTemporaryDir>
#include <QTextCursor>
#include <QtTest>
using namespace apollo;
using namespace apollo::editor;
class SpellingTests : public QObject {
  Q_OBJECT
private slots:
  void stale_results_unicode_and_structure() {
    QTemporaryDir dir;
    const std::vector<Element> elements = {
        {ElementType::cue, "JOHN"}, {ElementType::dialogue, "😀 beautifull\nDon't change this.", false, {{5, 15, Style::bold}}},
        {ElementType::cue, "JANE", true}, {ElementType::dialogue, "The color is blue."}};
    auto doc = screenplay_document(elements, QFont("monospace", 12));
    Spelling spelling(doc.get(), dir.filePath("words.json"));
    QTRY_VERIFY_WITH_TIMEOUT(!spelling.busy(), 10000);
    const auto original = document_elements(*doc);
    const auto saved = source::serialize_screenplay({0, original});
    spelling.request(12, 12, 12);
    QTRY_VERIFY(spelling.choices().contains("beautiful"));
    QVERIFY(spelling.replace("beautiful") >= 0);
    const auto edited = document_elements(*doc);
    QCOMPARE(edited[1].text, std::string("😀 beautiful\nDon't change this."));
    QCOMPARE(edited[1].id, original[1].id);
    QCOMPARE(edited[1].styles, std::vector<StyleSpan>({{5, 14, Style::bold}}));
    QCOMPARE(edited[2], original[2]);
    QCOMPARE(edited[3], original[3]);
    const auto reopened = source::parse_screenplay(source::serialize_screenplay({0, edited}));
    QCOMPARE(reopened.elements, edited);
    doc->undo();
    QCOMPARE(source::serialize_screenplay({0, document_elements(*doc)}), saved);
    spelling.request(12, 12, 12);
    QTextCursor cursor(doc.get()); cursor.setPosition(8); cursor.insertText("x");
    QTRY_VERIFY(!spelling.busy());
    QVERIFY(spelling.choices().isEmpty());
    QCOMPARE(spelling.replace("beautiful"), -1);
    // Layout notifications precede contentsChanged during some whole-document
    // edits. Previously published positions must be safe during that window.
    spelling.request(12, 12, 12);
    connect(doc->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            &spelling, [&] { spelling.marks(0, 10000); });
    cursor.select(QTextCursor::Document); cursor.insertText("Hi.");
    QCOMPARE(spelling.marks(0, 10000).size(), 0);
    // Explicit selected text is never silently widened to a whole word.
    spelling.request(2, 0, 2);
    QVERIFY(spelling.word().isEmpty());
  }
  void dictionary_failure_merge_and_cross_instance_refresh() {
    QTemporaryDir dir;
    const auto path = dir.filePath("words.json");
    auto one = screenplay_document({{ElementType::action, "Zorblax"}}, QFont("monospace", 12));
    auto two = screenplay_document({{ElementType::action, "Quorblax"}}, QFont("monospace", 12));
    Spelling first(one.get(), path), second(two.get(), path);
    QTRY_VERIFY(!first.busy() && !second.busy());
    first.request(3, 3, 3);
    QLockFile lock(path + ".lock"); QVERIFY(lock.tryLock());
    QVERIFY(!first.addWord());
    QVERIFY(!first.error().isEmpty());
    lock.unlock();
    QVERIFY(first.addWord());
    second.request(3, 3, 3); QVERIFY(second.addWord());
    QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
    const auto bytes = file.readAll(); file.close();
    QVERIFY(bytes.contains("zorblax") && bytes.contains("quorblax"));
    QTextCursor cursor(one.get()); cursor.select(QTextCursor::Document); cursor.insertText("Quorblax");
    QTRY_VERIFY_WITH_TIMEOUT(!first.busy() && first.misspellings().isEmpty(), 5000);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.close();
    first.request(3, 3, 3);
    QVERIFY(!first.addWord());
    QVERIFY(first.error().contains("Invalid"));
    QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray());
    file.close();
    // A bad personal file cannot disable the US dictionary or rewrite writing.
    auto fresh = screenplay_document({{ElementType::action, "beautifull"}}, QFont("monospace", 12));
    Spelling broken(fresh.get(), path);
    QTRY_VERIFY(!broken.busy());
    QCOMPARE(broken.misspellings().size(), 1);
    QVERIFY(!broken.error().isEmpty());
  }
  void checking_replacement_and_personal_words() {
    QTemporaryDir dir;
    auto doc = screenplay_document({{ElementType::action, "The color is beautifull. Zorblax."}}, QFont("monospace", 12));
    Spelling spelling(doc.get(), dir.filePath("words.json"));
    QTRY_VERIFY_WITH_TIMEOUT(!spelling.busy(), 10000);
    QVERIFY2(spelling.error().isEmpty(), qPrintable(spelling.error()));
    QCOMPARE(spelling.misspellings().size(), 2);
    QCOMPARE(spelling.misspellings()[0].toMap()["word"].toString(), QString("beautifull"));
    const auto before = document_elements(*doc);
    spelling.request(20, 20, 20);
    QTRY_VERIFY(spelling.choices().contains("beautiful"));
    QCOMPARE(document_elements(*doc)[0].text, before[0].text);
    QVERIFY(spelling.replace("beautiful") >= 0);
    QCOMPARE(doc->toPlainText(), QString("The color is beautiful. Zorblax."));
    doc->undo();
    QCOMPARE(doc->toPlainText(), QString::fromStdString(before[0].text));
    doc->redo();
    QTRY_VERIFY(!spelling.busy());
    spelling.request(28, 28, 28);
    QTRY_COMPARE(spelling.word(), QString("Zorblax"));
    QVERIFY(spelling.addWord());
    QTRY_VERIFY(!spelling.busy());
    QCOMPARE(spelling.misspellings().size(), 0);
    auto other = screenplay_document({{ElementType::dialogue, "ZORBLAX zorblax"}}, QFont("monospace", 12));
    Spelling reopened(other.get(), dir.filePath("words.json"));
    QTRY_VERIFY(!reopened.busy());
    QCOMPARE(reopened.misspellings().size(), 0);
  }
};
QTEST_MAIN(SpellingTests)
#include "spelling_tests.moc"
