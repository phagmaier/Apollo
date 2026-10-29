#include "source/fountain_export.h"
#include "source/fountain_conversion.h"
#include "source/fountain_intake.h"
#include "source/fountain_working.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <random>

using namespace apollo;
using namespace apollo::source;

namespace {
QByteArray bytes(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
using Elements = std::vector<Element>;
Element e(ElementType type, const char* text, std::vector<StyleSpan> styles = {}, bool dual = false) {
  return {type, text, dual, std::move(styles)};
}
const auto H = ElementType::heading, A = ElementType::action, C = ElementType::cue, P = ElementType::parenthetical,
           D = ElementType::dialogue, T = ElementType::transition;
}
Q_DECLARE_METATYPE(Elements)

class FountainExportTests : public QObject {
  Q_OBJECT
private slots:
  // The canonical text was written by hand from the semantic fixture and the
  // contract's spelling rules, not captured from the serializer.
  void writer_subset_exports_the_hand_written_canonical_text() {
    const auto model = parse_screenplay(bytes(APOLLO_FIXTURE_ROOT "/import/writer-subset.expected.json"));
    const auto result = export_fountain(model.elements, true);
    QVERIFY2(result.blockers.isEmpty(), qPrintable(result.blockers.join('\n')));
    QCOMPARE(result.bytes, bytes(APOLLO_FIXTURE_ROOT "/export/writer-subset.fountain"));
    // Without the title page only the screenplay body remains.
    const auto body = export_fountain(model.elements, false);
    QVERIFY(body.blockers.isEmpty());
    const auto expected = bytes(APOLLO_FIXTURE_ROOT "/export/writer-subset.fountain");
    QCOMPARE(body.bytes, expected.mid(expected.indexOf("INT. PLATFORM - DAY\n")));
    // Authored lowercase headings and cue extensions are written in capitals.
    const auto cased = parse_screenplay(bytes(APOLLO_FIXTURE_ROOT "/import/case-subset.expected.json"));
    const auto capitals = export_fountain(cased.elements, true);
    QVERIFY2(capitals.blockers.isEmpty(), qPrintable(capitals.blockers.join('\n')));
    QCOMPARE(capitals.bytes, bytes(APOLLO_FIXTURE_ROOT "/export/case-subset.fountain"));
    QCOMPARE(fountain_case_note(cased.elements),
             QString("Written in capitals, as Apollo shows and prints them: 3 scene headings and 3 character cues typed in lowercase. "
                     "The project keeps the case as typed."));
  }

  // Action is unforced only when its first line begins with a letter or digit,
  // has lowercase before any "(" and could not be a heading or title key.
  void spelling_forces_only_where_plain_text_would_change_meaning_data() {
    QTest::addColumn<Elements>("snapshot");
    QTest::addColumn<bool>("title");
    QTest::addColumn<QByteArray>("expected");
    QTest::newRow("capital action") << Elements{e(A, "BANG.")} << true << QByteArray("!BANG.\n");
    QTest::newRow("action like a heading") << Elements{e(A, "Int. the hall is dark.")} << true << QByteArray("!Int. the hall is dark.\n");
    QTest::newRow("capitals before parenthesis") << Elements{e(A, "A (b) c")} << true << QByteArray("!A (b) c\n");
    QTest::newRow("action punctuation first") << Elements{e(A, "...and then.")} << true << QByteArray("!...and then.\n");
    QTest::newRow("title-like opening line") << Elements{e(A, "Note: the door sticks.")} << true << QByteArray("!Note: the door sticks.\n");
    QTest::newRow("title-like after title page") << Elements{e(ElementType::title, "X"), e(A, "Note: the door sticks.")} << true
        << QByteArray("Title: X\n\nNote: the door sticks.\n");
    QTest::newRow("title page omitted") << Elements{e(ElementType::title, "X"), e(A, "Note: the door sticks.")} << false
        << QByteArray("!Note: the door sticks.\n");
    QTest::newRow("multiline action") << Elements{e(A, "He waits.\nNOTHING.\nStill nothing.")} << true
        << QByteArray("He waits.\nNOTHING.\nStill nothing.\n");
    QTest::newRow("forced heading") << Elements{e(H, "FLASHBACK - THE PIER")} << true << QByteArray(".FLASHBACK - THE PIER\n");
    QTest::newRow("natural heading prefixes") << Elements{e(H, "I/E CAR - MOVING"), e(H, "INT./EXT. VAN - DAY"), e(H, "EST. CITY")} << true
        << QByteArray("I/E CAR - MOVING\n\nINT./EXT. VAN - DAY\n\nEST. CITY\n");
    QTest::newRow("title-like opening heading") << Elements{e(H, "EXT: ROOF")} << false << QByteArray(".EXT: ROOF\n");
    QTest::newRow("title-like opening cue") << Elements{e(C, "TO:DAY"), e(D, "Go.")} << false << QByteArray("@TO:DAY\nGo.\n");
    QTest::newRow("cue punctuation first") << Elements{e(C, "'NICK'"), e(D, "Hi.")} << true << QByteArray("@'NICK'\nHi.\n");
    QTest::newRow("numbered cue") << Elements{e(C, "3RD MAN (O.S.)"), e(P, "(off)"), e(D, "Over here."), e(P, "(beat)"), e(D, "Now.")} << true
        << QByteArray("3RD MAN (O.S.)\n(off)\nOver here.\n(beat)\nNow.\n");
    QTest::newRow("dual pair") << Elements{e(C, "A"), e(D, "One."), e(C, "B", {}, true), e(D, "Two.")} << true
        << QByteArray("A\nOne.\n\nB ^\nTwo.\n");
    QTest::newRow("mixed-case transition") << Elements{e(T, "Smash cut to:")} << true << QByteArray(">Smash cut to:\n");
    QTest::newRow("underline around bold") << Elements{e(A, "the big dog", {{0, 11, Style::underline}, {4, 7, Style::bold}})} << true
        << QByteArray("!_the **big** dog_\n");
    QTest::newRow("italic inside bold") << Elements{e(A, "the big dog", {{0, 11, Style::bold}, {4, 7, Style::italic}})} << true
        << QByteArray("!**the *big* dog**\n");
    QTest::newRow("all three styles") << Elements{e(A, "So x.", {{3, 4, Style::bold}, {3, 4, Style::italic}, {3, 4, Style::underline}})} << true
        << QByteArray("So _***x***_.\n");
    QTest::newRow("emphasis ending inside word") << Elements{e(A, "doors open", {{0, 4, Style::bold}})} << true
        << QByteArray("!**door**s open\n");
    QTest::newRow("literal markers") << Elements{e(A, "a\\*b_c\\d")} << true << QByteArray("a\\\\\\*b\\_c\\d\n");
    QTest::newRow("several authors") << Elements{e(ElementType::author, "Ada"), e(ElementType::author, "Bo"), e(ElementType::source, "Story")} << true
        << QByteArray("Author:\n   Ada\n   Bo\nSource: Story\n");
    QTest::newRow("break and centered") << Elements{e(ElementType::page_break, ""), e(ElementType::centered, "THE END")} << true
        << QByteArray("===\n\n> THE END <\n");
    // Headings and cues are written as presented: ASCII letters in capitals.
    QTest::newRow("lowercase headings") << Elements{e(H, "int. hall - day"), e(H, "flashback"), e(H, "i/e car")} << true
        << QByteArray("INT. HALL - DAY\n\n.FLASHBACK\n\nI/E CAR\n");
    QTest::newRow("lowercase opening heading key") << Elements{e(H, "ext: roof")} << false << QByteArray(".EXT: ROOF\n");
    QTest::newRow("lowercase cues and extensions") << Elements{e(C, "mara (v.o.)"), e(D, "One."), e(C, "Bo (cont'd)", {}, true), e(D, "Two.")} << true
        << QByteArray("MARA (V.O.)\nOne.\n\nBO (CONT'D) ^\nTwo.\n");
    QTest::newRow("emphasized lowercase heading") << Elements{e(H, "int. hall", {{5, 9, Style::underline}})} << true << QByteArray("INT. _HALL_\n");
    QTest::newRow("lowercase cue punctuation first") << Elements{e(C, "'nick'"), e(D, "Hi.")} << true << QByteArray("@'NICK'\nHi.\n");
    QTest::newRow("action and dialogue keep their case") << Elements{e(H, "int. a"), e(A, "Mara waits."), e(C, "mara"), e(D, "Here, mara.")} << true
        << QByteArray("INT. A\n\nMara waits.\n\nMARA\nHere, mara.\n");
  }
  void spelling_forces_only_where_plain_text_would_change_meaning() {
    QFETCH(Elements, snapshot);
    QFETCH(bool, title);
    QFETCH(QByteArray, expected);
    const auto result = export_fountain(snapshot, title);
    QVERIFY2(result.blockers.isEmpty(), qPrintable(result.blockers.join('\n')));
    QCOMPARE(result.bytes, expected);
    QVERIFY(fountain_export_blockers(snapshot, title).isEmpty());
  }

  void unsupported_snapshots_are_blocked_with_a_located_reason_data() {
    QTest::addColumn<Elements>("snapshot");
    QTest::addColumn<QString>("reason");
    QTest::newRow("cue letter not shown in capitals") << Elements{e(H, "INT. A"), e(C, "jos\xc3\xa9"), e(D, "Hi.")}
        << QString("Element 2, Character cue “josé”: contains “é”, which Apollo does not yet show in capitals in scene headings and character cues.");
    QTest::newRow("heading letter not shown in capitals") << Elements{e(H, "INT. CAF\xc3\xa9")} << QString("Element 1, Scene heading “INT. CAFé”: contains “é”");
    QTest::newRow("unverified character") << Elements{e(A, "Wait—now.")} << QString("Element 1, Action “Wait—now.”: contains “—” (U+2014)");
    QTest::newRow("empty element") << Elements{e(A, "Done."), e(A, "")} << QString("Element 2, Action (empty): is empty; type its text or delete it.");
    QTest::newRow("repeated spaces") << Elements{e(A, "Two  spaces.")} << QString("leading, trailing or repeated spaces");
    QTest::newRow("long title") << Elements{e(ElementType::title, "A TITLE THAT IS MUCH LONGER THAN FORTY EIGHT CHARACTERS")} << QString("longer than 48");
    QTest::newRow("multiline lyric") << Elements{e(ElementType::lyric, "La.\nLa.")} << QString("Lyric “La.”: contains a line break");
    QTest::newRow("orphan dialogue") << Elements{e(A, "Rain."), e(D, "Hello.")} << QString("Element 2, Dialogue “Hello.”: is not part of a speech");
    QTest::newRow("cue without dialogue") << Elements{e(C, "MAX"), e(P, "(nods)"), e(A, "Rain.")} << QString("Element 1, Character cue “MAX”: has no dialogue.");
    QTest::newRow("adjacent dialogue") << Elements{e(C, "MAX"), e(D, "One."), e(D, "Two.")} << QString("Element 3, Dialogue “Two.”: follows another dialogue element");
    QTest::newRow("lonely dual") << Elements{e(A, "Rain."), e(C, "MAX", {}, true), e(D, "Yes.")} << QString("is marked as dual dialogue");
    QTest::newRow("bare parenthetical") << Elements{e(C, "MAX"), e(P, "quietly"), e(D, "Yes.")} << QString("must begin with “(” and end with “)”");
    QTest::newRow("scene number") << Elements{e(H, "INT. ROOM #4")} << QString("reads as a scene number");
    QTest::newRow("heading punctuation") << Elements{e(H, "(FLASHBACK)")} << QString("must begin with INT.");
    QTest::newRow("crossing styles") << Elements{e(A, "ab cd ef", {{0, 5, Style::bold}, {3, 8, Style::underline}})} << QString("overlapping emphasis");
    QTest::newRow("touching styles") << Elements{e(A, "ab.cd", {{0, 2, Style::italic}, {2, 5, Style::bold}})} << QString("touch or share a boundary");
    QTest::newRow("shared start") << Elements{e(A, "abcd", {{0, 4, Style::bold}, {0, 2, Style::italic}})} << QString("touch or share a boundary");
    QTest::newRow("across break") << Elements{e(A, "ab\ncd", {{1, 4, Style::italic}})} << QString("includes a line break");
    QTest::newRow("styled break only") << Elements{e(A, "ab\ncd", {{2, 3, Style::bold}})} << QString("includes a line break");
    QTest::newRow("inside word") << Elements{e(A, "unbelievable", {{2, 8, Style::italic}})} << QString("begins inside a word");
    QTest::newRow("spaced emphasis") << Elements{e(A, "a b", {{1, 3, Style::italic}})} << QString("starts or ends with a space");
    QTest::newRow("note marker") << Elements{e(A, "See [[this]].")} << QString("would contain “[[”");
    QTest::newRow("boneyard via emphasis") << Elements{e(A, "and/or now", {{4, 6, Style::italic}})} << QString("would contain “/*”");
    QTest::newRow("capital TO:") << Elements{e(A, "GO TO:")} << QString("reads as a transition");
    QTest::newRow("inner markup line") << Elements{e(A, "Look.\n#1 again.")} << QString("has a line beginning with “#”");
    QTest::newRow("inner forced heading") << Elements{e(A, "Look.\n.\xc3\xa9tude")} << QString("reads as a scene heading");
    QTest::newRow("inner parenthetical line") << Elements{e(C, "MAX"), e(D, "Well.\n(aside)")} << QString("dialogue line in parentheses");
    QTest::newRow("generated marker") << Elements{e(ElementType::more, "(MORE)")} << QString("generated layout marker");
    QTest::newRow("transition arrow") << Elements{e(T, "BACK TO <")} << QString("reads as centered text");
    QTest::newRow("dual marker text") << Elements{e(C, "MAX^"), e(D, "Yes.")} << QString("dual-dialogue marker");
    QTest::newRow("extension only cue") << Elements{e(C, "(V.O.)"), e(D, "Yes.")} << QString("needs a letter");
  }
  void unsupported_snapshots_are_blocked_with_a_located_reason() {
    QFETCH(Elements, snapshot);
    QFETCH(QString, reason);
    const auto checked = fountain_export_blockers(snapshot, true);
    const auto result = export_fountain(snapshot, true);
    QVERIFY(result.bytes.isEmpty());
    QCOMPARE(result.blockers, checked);
    QVERIFY2(checked.join('\n').contains(reason), qPrintable(checked.join('\n')));
  }

  // A pair is two adjacent speeches; a third dual cue has no single partner.
  void dual_dialogue_pairs_with_one_preceding_speech() {
    Elements snapshot{e(C, "MAX"), e(D, "Yes."), e(C, "ZED", {}, true), e(D, "No.")};
    QVERIFY(export_fountain(snapshot, true).blockers.isEmpty());
    snapshot.push_back(e(C, "AMY", {}, true)); snapshot.push_back(e(D, "Maybe."));
    QCOMPARE(fountain_export_blockers(snapshot, true),
             QStringList{"Element 5, Character cue “AMY”: is marked as dual dialogue but does not directly follow a single speech to pair with."});
  }

  void title_page_checks_follow_the_title_choice() {
    const Elements snapshot{e(ElementType::credit, "written by"), e(ElementType::title, "lowercase title"), e(A, "Rain.")};
    QVERIFY(fountain_export_blockers(snapshot, true).join('\n').contains("Element 2, Title “lowercase title”"));
    const auto body = export_fountain(snapshot, false);
    QVERIFY2(body.blockers.isEmpty(), qPrintable(body.blockers.join('\n')));
    QCOMPARE(body.bytes, QByteArray("Rain.\n"));
  }

  // Edits after import can make the same project ineligible and eligible again.
  void eligibility_is_evaluated_on_each_snapshot() {
    auto model = parse_screenplay(bytes(APOLLO_FIXTURE_ROOT "/import/writer-subset.expected.json")).elements;
    QVERIFY(fountain_export_blockers(model, true).isEmpty());
    model[11].text = "June";
    QVERIFY(export_fountain(model, true).bytes.contains("\nJUNE ^\n"));
    model[11].text = "Jun\xc3\xa9";
    QCOMPARE(fountain_export_blockers(model, true),
             QStringList{"Element 12, Character cue “Juné”: contains “é”, which Apollo does not yet show in capitals in scene headings and character cues."});
    model[11].text = "JUNE";
    model[12].text = "At the \nsame time.";
    QVERIFY(fountain_export_blockers(model, true).join('\n').contains("Element 13, Dialogue “At the ”"));
    model[12].text = "At the\nsame time.";
    const auto result = export_fountain(model, true);
    QVERIFY(result.blockers.isEmpty());
    QVERIFY(result.bytes.contains("JUNE ^\nAt the\nsame time.\n\n~A distant song."));
  }

  // The disclosure counts headings and cues whose authored case differs from
  // what is written; title fields and other elements keep their text.
  void case_presentation_is_disclosed() {
    QVERIFY(fountain_case_note({e(H, "INT. A"), e(C, "MARA"), e(D, "hi.")}).isEmpty());
    QCOMPARE(fountain_case_note({e(H, "int. a"), e(A, "rain."), e(C, "Mara"), e(D, "hi."), e(C, "BO (v.o.)"), e(D, "yo."), e(ElementType::title, "x")}),
             QString("Written in capitals, as Apollo shows and prints them: 1 scene heading and 2 character cues typed in lowercase. "
                     "The project keeps the case as typed."));
    QCOMPARE(fountain_case_note({e(H, "int. a"), e(H, "ext. b")}),
             QString("Written in capitals, as Apollo shows and prints them: 2 scene headings typed in lowercase. The project keeps the case as typed."));
  }

  // Imported retained material is summarized for the disclosure; ordinary
  // projects have nothing to disclose.
  void omitted_imported_material_is_counted_from_the_retained_records() {
    QTemporaryDir root;
    const auto input = root.filePath("in.fountain");
    replace_file(input, bytes(APOLLO_FIXTURE_ROOT "/import/writer-subset.fountain"));
    const auto working = editable_fountain_assets(read_fountain_intake(input).assets);
    QCOMPARE(fountain_export_omissions(working),
             QString("Not included in clean Fountain: 1 imported note, 1 stash passage, 1 unsupported title-page field, 1 section, "
                     "2 synopses, 1 production scene number. They stay in this project's imported material and original."));
    QVERIFY(fountain_export_omissions({}).isEmpty());
    QVERIFY(fountain_export_omissions(interpreted_fountain_assets(read_fountain_intake(input).assets)).isEmpty());
  }

  // Every refusal has an explicit, located reason: on random snapshots built
  // from Fountain-significant pieces, the read-back guard never blocks alone.
  void explicit_checks_explain_every_refusal() {
    std::mt19937 random(11);
    const char* pieces[] = {"a", "B", "x", " ", ".", "!", "#", "=", ">", "<", "@", "~", "(", ")", "*", "_", "\\", "/", "[", "]",
                            ":", "^", "TO:", "INT. ", "\xc3\xa9", "1", "\n", "-", "'", "Note:", "I/E ", "EXT"};
    const ElementType types[] = {H, A, C, P, D, ElementType::lyric, T, ElementType::centered, ElementType::page_break,
                                 ElementType::title, ElementType::author};
    int eligible = 0;
    for (int round = 0; round < 40000; ++round) {
      Elements snapshot;
      for (int i = 0, n = 1 + int(random() % 4); i < n; ++i) {
        Element element{types[random() % std::size(types)], ""};
        for (int j = 0, length = int(random() % 6); j < length && element.type != ElementType::page_break; ++j)
          element.text += pieces[random() % std::size(pieces)];
        if (!element.text.empty() && random() % 3 == 0) {
          auto boundary = [&](std::size_t x) { while (x < element.text.size() && (uchar(element.text[x]) & 0xc0) == 0x80) ++x; return x; };
          const auto a = random() % element.text.size();
          const auto start = boundary(a), end = boundary(a + 1 + random() % (element.text.size() - a));
          if (start < end) element.styles.push_back({start, end, Style(random() % 3)});
        }
        element.dual_right = element.type == C && random() % 4 == 0;
        snapshot.push_back(element);
      }
      const bool title = random() % 2;
      if (!fountain_export_blockers(snapshot, title).isEmpty()) continue;
      const auto result = export_fountain(snapshot, title);
      QVERIFY2(result.blockers.isEmpty(), qPrintable(result.blockers.join('\n')));
      ++eligible;
    }
    QVERIFY(eligible > 5000);
  }

  // Coarse bound check: the verifying reader is limited to 1 MiB.
  void oversized_text_is_blocked_by_the_verifying_reader_bound() {
    Elements snapshot;
    std::string paragraph(1000, 'a');
    for (int i = 0; i < 1100; ++i) snapshot.push_back(e(A, paragraph.c_str()));
    QVERIFY(export_fountain(snapshot, true).blockers.join('\n').contains("exceed 1 MiB"));
  }
};

QTEST_GUILESS_MAIN(FountainExportTests)
#include "fountain_export_tests.moc"
