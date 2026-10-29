#include "source/fountain_conversion.h"
#include "source/fountain_intake.h"
#include "source/fountain_working.h"
#include "source/project_access.h"
#include "source/recovery.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace apollo;
using namespace apollo::source;
namespace {
QByteArray bytes(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("missing fixture");
  return file.readAll();
}
QString type_name(ElementType type) {
  switch (type) {
    case ElementType::heading: return "sceneHeading";
    case ElementType::action: return "action";
    case ElementType::cue: return "character";
    case ElementType::parenthetical: return "parenthetical";
    case ElementType::dialogue: return "dialogue";
    case ElementType::lyric: return "lyric";
    case ElementType::transition: return "transition";
    case ElementType::centered: return "centered";
    case ElementType::page_break: return "pageBreak";
    case ElementType::title: return "title";
    case ElementType::author: return "author";
    case ElementType::draft_date: return "draftDate";
    case ElementType::credit: return "credit";
    case ElementType::source: return "source";
    case ElementType::contact: return "contact";
    default: return "invalid";
  }
}
FountainIntake intake(const QByteArray& original, const QString& parent) {
  const auto path = parent + "/input.fountain";
  replace_file(path, original);
  return read_fountain_intake(path);
}
QStringList codes(const std::vector<FountainIssue>& issues) {
  QStringList result;
  for (const auto& issue : issues) result << issue.code;
  return result;
}
}
class FountainConversionTests : public QObject {
  Q_OBJECT
private slots:
  void writer_subset_matches_independent_screenplay_and_associations() {
    const auto root = QStringLiteral(APOLLO_FIXTURE_ROOT "/import/");
    const auto captured = read_fountain_intake(root + "writer-subset.fountain");
    const auto expected = parse_screenplay(bytes(root + "writer-subset.expected.json"));
    const auto c = convert_fountain(captured.assets);
    QCOMPARE(canonical(c.screenplay()), expected);
    const auto records = QJsonDocument::fromJson(bytes(root + "writer-subset.records.expected.json")).array();
    const auto actual = c.json().value("retained").toArray();
    QCOMPARE(actual.size(), records.size());
    for (qsizetype i = 0; i < records.size(); ++i) {
      const auto want = records[i].toObject(), got = actual[i].toObject();
      for (auto it = want.begin(); it != want.end(); ++it) QCOMPARE(got.value(it.key()), it.value());
      QVERIFY(!got.value("name").toString().isEmpty());
    }
    QCOMPARE(fountain_edit_blockers(captured.assets), QStringList{});
    const auto assets = editable_fountain_assets(captured.assets);
    QCOMPARE(initial_fountain_writing(assets), expected);
    QTemporaryDir parent;
    const auto project = create_project(parent.filePath("working"), expected, assets);
    QVERIFY(project.import_working && !project.import_review);
    QCOMPARE(open_project(project.directory).assets, assets);
    // Metadata cannot grant access by flipping a boolean or contract revision.
    for (const auto* key : {"contract", "version", "unknown"}) {
      auto bad = assets;
      auto json = QJsonDocument::fromJson(*bad.imported_material).object();
      json.insert(key, 2); bad.imported_material = QJsonDocument(json).toJson();
      QVERIFY_THROWS_EXCEPTION(SourceError, validate_fountain_working(bad));
    }
    ProjectSession session(project.directory);
    replace_file(project.directory + "/imports/original.fountain", "Changed evidence");
    QVERIFY_THROWS_EXCEPTION(SourceError, session.save(serialize_screenplay(expected)));
    QVERIFY_THROWS_EXCEPTION(SourceError, session.read_reload());
    QCOMPARE(bytes(project.screenplay_path()), serialize_screenplay(expected));
    // Cached dependencies still protect the original imported records in a copy.
    const auto copy = create_project(parent.filePath("copy"), expected, assets);
    QCOMPARE(open_project(copy.directory).assets, assets);
  }
  // writer-subset-2: lowercase headings and cue extensions keep their authored
  // text and original bytes; presentation is in capitals. A copy cannot claim
  // the older writer-subset-1, which required capitals, to reopen.
  void writer_subset_admits_authored_lowercase_headings_and_extensions() {
    const auto root = QStringLiteral(APOLLO_FIXTURE_ROOT "/import/");
    const auto captured = read_fountain_intake(root + "case-subset.fountain");
    const auto expected = parse_screenplay(bytes(root + "case-subset.expected.json"));
    QCOMPARE(canonical(convert_fountain(captured.assets).screenplay()), expected);
    QCOMPARE(fountain_edit_blockers(captured.assets), QStringList{});
    QVERIFY(!fountain_edit_blockers(captured.assets, "writer-subset-1").isEmpty());
    const auto assets = editable_fountain_assets(captured.assets);
    QCOMPARE(QJsonDocument::fromJson(*assets.imported_material).object().value("contract").toString(), QString("writer-subset-4"));
    QCOMPARE(*assets.original_fountain, bytes(root + "case-subset.fountain"));
    QCOMPARE(initial_fountain_writing(assets), expected);
    auto older = assets;
    auto json = QJsonDocument::fromJson(*older.imported_material).object();
    // A copy made under writer-subset-2 reopens by its rules: this has no spacing it refused.
    json.insert("contract", "writer-subset-2"); older.imported_material = QJsonDocument(json).toJson();
    validate_fountain_working(older);
    json.insert("contract", "writer-subset-1"); older.imported_material = QJsonDocument(json).toJson();
    try { validate_fountain_working(older); QFAIL("a writer-subset-1 claim admitted lowercase headings"); }
    catch (const SourceError& error) { QVERIFY2(QString(error.what()).contains("f1: authored case requires review."), error.what()); }
    // The writer-subset fixture made under writer-subset-1 still reopens by its rules.
    auto legacy = editable_fountain_assets(read_fountain_intake(root + "writer-subset.fountain").assets);
    json = QJsonDocument::fromJson(*legacy.imported_material).object();
    json.insert("contract", "writer-subset-1"); legacy.imported_material = QJsonDocument(json).toJson();
    validate_fountain_working(legacy);
  }
  // writer-subset-3 (docs/spacing.md): spaces at dialogue line edges, a line
  // of spaces inside dialogue and repeated spaces in action and dialogue keep
  // their authored text and the original bytes. Dialogue indentation is not
  // retained, as Fountain ignores it. writer-subset-2 refused this spacing.
  void writer_subset_admits_spacing_with_a_kept_meaning() {
    const auto root = QStringLiteral(APOLLO_FIXTURE_ROOT "/import/");
    const auto captured = read_fountain_intake(root + "spacing-subset.fountain");
    const auto expected = parse_screenplay(bytes(root + "spacing-subset.expected.json"));
    QCOMPARE(canonical(convert_fountain(captured.assets).screenplay()), expected);
    QCOMPARE(fountain_edit_blockers(captured.assets), QStringList{});
    QCOMPARE(fountain_edit_blockers(captured.assets, "writer-subset-2"),
             (QStringList{"f2: empty or significant spacing requires review.", "f4: empty or significant spacing requires review.",
                          "f7: empty or significant spacing requires review."}));
    const auto assets = editable_fountain_assets(captured.assets);
    QCOMPARE(QJsonDocument::fromJson(*assets.imported_material).object().value("contract").toString(), QString("writer-subset-4"));
    QCOMPARE(*assets.original_fountain, bytes(root + "spacing-subset.fountain"));
    QCOMPARE(initial_fountain_writing(assets), expected);
    auto older = assets;
    auto json = QJsonDocument::fromJson(*older.imported_material).object();
    json.insert("contract", "writer-subset-2"); older.imported_material = QJsonDocument(json).toJson();
    try { validate_fountain_working(older); QFAIL("a writer-subset-2 claim admitted spacing"); }
    catch (const SourceError& error) { QVERIFY2(QString(error.what()).contains("f4: empty or significant spacing requires review."), error.what()); }
    json.insert("contract", "writer-subset-3"); older.imported_material = QJsonDocument(json).toJson();
    validate_fountain_working(older);
  }
  void punctuation_keeps_unicode_styles_and_legacy_admission() {
    const auto root = QStringLiteral(APOLLO_FIXTURE_ROOT "/import/punctuation-subset");
    const auto captured = read_fountain_intake(root + ".fountain");
    const auto expected = parse_screenplay(bytes(root + ".expected.json"));
    QCOMPARE(canonical(convert_fountain(captured.assets).screenplay()), expected);
    QCOMPARE(fountain_edit_blockers(captured.assets), QStringList{});
    for (const auto* contract : {"writer-subset-1", "writer-subset-2", "writer-subset-3"})
      QVERIFY(fountain_edit_blockers(captured.assets, contract).join('\n').contains("character outside the verified writer subset"));
    const auto assets = editable_fountain_assets(captured.assets);
    QCOMPARE(initial_fountain_writing(assets), expected);
    QCOMPARE(*assets.original_fountain, bytes(root + ".fountain"));
    auto legacy = assets;
    auto json = QJsonDocument::fromJson(*legacy.imported_material).object();
    json.insert("contract", "writer-subset-3");
    legacy.imported_material = QJsonDocument(json).toJson();
    QVERIFY_THROWS_EXCEPTION(SourceError, validate_fountain_working(legacy));
  }
  void writer_subset_explains_case_it_cannot_present() {
    QTemporaryDir parent;
    QCOMPARE(fountain_edit_blockers(intake("@McKay (v.o.)\nWords.", parent.path()).assets),
             QStringList{"f1: forced mixed-case character name requires review; Apollo presents cues in capitals."});
    QCOMPARE(fountain_edit_blockers(intake("int. caf\xc3\xa9 - day\n\nAction.", parent.path()).assets),
             QStringList{"f1: a letter that is not presented in capitals requires review."});
  }
  void writer_subset_rejects_unverified_material_data() {
    QTest::addColumn<QByteArray>("original");
    for (const auto& original : {QByteArray("@McKay\nWords."), QByteArray("int. caf\xc3\xa9 - day\n\nAction."), QByteArray("@JOS\xc3\xa9\nWords."),
        QByteArray("!  Indented."), QByteArray("!Trailing. "), QByteArray(".INT.  ROOM"), QByteArray("@MARA  JONES\nWords."),
        QByteArray("@MARA\n\tWords."), QByteArray("!Before.\n\n\n!After."),
        QByteArray("[[bad"), QByteArray("A\n~Lyric in speech."), QByteArray("!Unsupported \xf0\x9f\x98\x80"),
        QByteArray("A\n(beat)\n  \nFirst."), QByteArray("A\nLast.\n  "), QByteArray("!Cross *one _two* three_"), QByteArray(65537, 'x'),
        QByteArray("!Visible.") + QByteArray("[[n]]").repeated(257)})
      QTest::newRow(original.left(28).toHex().constData()) << original;
  }
  void writer_subset_rejects_unverified_material() {
    QFETCH(QByteArray, original);
    QTemporaryDir parent;
    const auto captured = intake(original, parent.path());
    QVERIFY(!fountain_edit_blockers(captured.assets).empty());
    QVERIFY_THROWS_EXCEPTION(SourceError, editable_fountain_assets(captured.assets));
    QCOMPARE(*captured.assets.original_fountain, original);
  }
  void independent_and_official_interpretations_data() {
    QTest::addColumn<QString>("name");
    const auto expected = QJsonDocument::fromJson(bytes(QStringLiteral(APOLLO_FIXTURE_ROOT "/import/interpretations.expected.json"))).object();
    for (auto it = expected.begin(); it != expected.end(); ++it) QTest::newRow(qPrintable(it.key())) << it.key();
  }
  void independent_and_official_interpretations() {
    QFETCH(QString, name);
    const auto root = QStringLiteral(APOLLO_FIXTURE_ROOT);
    const auto expected = QJsonDocument::fromJson(bytes(root + "/import/interpretations.expected.json")).object().value(name).toObject();
    const auto path = root + (name == "independent" ? "/import/" : "/fountain-syntax/") + name + ".fountain";
    const auto original = bytes(path);
    const auto captured = read_fountain_intake(path);
    const auto converted = convert_fountain(captured.assets);
    QJsonArray actual;
    for (const auto& e : converted.elements) {
      QJsonArray row{type_name(e.element.type), QString::fromUtf8(e.element.text)};
      if (e.element.dual_right) row.append(true);
      actual.append(row);
      QVERIFY(e.range.start >= 0 && e.range.start <= e.range.end && e.range.end <= original.size());
      for (const auto& source : e.text_sources) {
        QVERIFY(source.start >= e.range.start && source.end <= e.range.end);
        QVERIFY(QString::fromUtf8(original.mid(source.start, source.end - source.start)).toUtf8() == original.mid(source.start, source.end - source.start));
      }
    }
    QCOMPARE(actual, expected.value("elements").toArray());
    QJsonObject counts;
    for (const auto& r : converted.records) counts.insert(r.kind, counts.value(r.kind).toInt() + 1);
    QCOMPARE(counts, expected.value("records").toObject());
    QCOMPARE(QJsonArray::fromStringList(codes(converted.unresolved)), expected.value("unresolved").toArray());
    // Independent source schema round-trip checks offsets and supported text,
    // without claiming a Fountain serializer or layout oracle.
    QCOMPARE(parse_screenplay(serialize_screenplay(converted.screenplay())), canonical(converted.screenplay()));
    const auto stored = interpreted_fountain_assets(captured.assets);
    validate_fountain_intake(stored);
    QCOMPARE(*stored.original_fountain, original);
    QCOMPARE(convert_fountain(stored).json(), converted.json());
    QCOMPARE(bytes(path), original);
  }
  void located_unicode_styles_notes_and_metadata() {
    QTemporaryDir parent;
    const auto original = QByteArray("\xef\xbb\xbf" "Draft date: today\r\nUnknown: keep\r\n\r\n!caf\xc3\xa9 [[<b>note</b>]] **bold** _u_\r\nline two\r\n\r\n/* stash\r\ntext */\r\n");
    const auto c = convert_fountain(intake(original, parent.path()).assets);
    QCOMPARE(c.elements.size(), std::size_t(2));
    QCOMPARE(c.elements[1].element.text, std::string("café  bold u\nline two"));
    QCOMPARE(c.elements[1].element.styles, (std::vector<StyleSpan>{{7, 11, Style::bold}, {12, 13, Style::underline}}));
    QCOMPARE(c.elements[1].range.first_line, 4);
    QCOMPARE(c.elements[1].range.last_line, 5);
    const auto note = std::find_if(c.records.begin(), c.records.end(), [](const auto& r) { return r.kind == "note"; });
    QVERIFY(note != c.records.end());
    QCOMPARE(note->range.start, original.indexOf("[["));
    QCOMPARE(note->range.end, original.indexOf("]]") + 2);
    QCOMPARE(note->text, QString("<b>note</b>"));
    QCOMPARE(note->anchor, QString::fromStdString(c.elements[1].element.id));
    QVERIFY(note->name.contains("note 1 (line 4)"));
    for (const auto& slice : c.elements[1].text_sources)
      QVERIFY(slice.end <= note->range.start || slice.start >= note->range.end);
    const auto stash = std::find_if(c.records.begin(), c.records.end(), [](const auto& r) { return r.kind == "stash"; });
    QVERIFY(stash != c.records.end());
    QCOMPARE(stash->text, QString(" stash\r\ntext "));
    QCOMPARE(stash->range.first_line, 7);
    QCOMPARE(stash->range.last_line, 8);
    const auto metadata = std::find_if(c.records.begin(), c.records.end(), [](const auto& r) { return r.kind == "titleMetadata"; });
    QVERIFY(metadata != c.records.end());
    QCOMPARE(metadata->key, QString("Unknown"));
    QCOMPARE(metadata->text, QString("Unknown: keep"));
    QVERIFY(c.unresolved.empty());
    const auto saved_styles = c.json().value("screenplay").toObject().value("elements").toArray()[0].toObject().value("styles").toArray();
    QCOMPARE(saved_styles, (QJsonArray{QJsonObject{{"start", 6}, {"end", 10}, {"style", "bold"}}, QJsonObject{{"start", 11}, {"end", 12}, {"style", "underline"}}}));
  }
  void bounded_malformed_ranges_data() {
    QTest::addColumn<QByteArray>("original");
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("start"); QTest::addColumn<int>("end");
    QTest::newRow("note-paragraph") << QByteArray("[[bad\r\n\r\nVisible.") << QString("unclosed-delimiter") << 0 << 5;
    QTest::newRow("nested-note") << QByteArray("[[a [[b]]\n\nVisible.") << QString("nested-delimiter") << 0 << 9;
    QTest::newRow("unclosed-stash") << QByteArray("A.\n\n/*bad\n\nNot safe to guess.") << QString("unclosed-delimiter") << 4 << 29;
    QTest::newRow("orphan") << QByteArray("A ]] literal.") << QString("orphan-delimiter") << 2 << 4;
    QTest::newRow("utf8-control") << QByteArray("caf\xc3\xa9\xe2\x80\xae.") << QString("control-character") << 5 << 8;
    QTest::newRow("encoding") << QByteArray("\xff\xfe", 2) << QString("encoding") << 0 << 2;
  }
  void bounded_malformed_ranges() {
    QFETCH(QByteArray, original); QFETCH(QString, code); QFETCH(int, start); QFETCH(int, end);
    QTemporaryDir parent;
    const auto c = convert_fountain(intake(original, parent.path()).assets);
    QVERIFY(!c.unresolved.empty());
    QCOMPARE(c.unresolved.front().code, code);
    QCOMPARE(c.unresolved.front().range.start, start);
    QCOMPARE(c.unresolved.front().range.end, end);
    if (original.contains("Visible.")) QCOMPARE(c.elements.back().element.text, std::string("Visible."));
    QVERIFY(!c.json().value("editable").toBool());
    QVERIFY(!c.json().value("fountainExport").toBool());
    QVERIFY(!c.json().value("pdfExport").toBool());
  }
  void emphasis_fallback_and_combinations() {
    QTemporaryDir parent;
    const auto c = convert_fountain(intake("!***Both*** _**B** and *I*_ \\*literal\\*\n\nHe dialed *69 and then *23.\n\n!unmatched * stays\nnext * stays\n", parent.path()).assets);
    QCOMPARE(c.elements[0].element.text, std::string("Both B and I *literal*"));
    QCOMPARE(canonical_styles(c.elements[0].element.styles), canonical_styles({{0, 4, Style::italic}, {0, 4, Style::bold}, {5, 6, Style::bold}, {11, 12, Style::italic}, {5, 12, Style::underline}}));
    QCOMPARE(c.elements[1].element.text, std::string("He dialed *69 and then *23."));
    QVERIFY(c.elements[1].element.styles.empty());
    QCOMPARE(c.elements[2].element.text, std::string("unmatched * stays\nnext * stays"));
    QVERIFY(c.elements[2].element.styles.empty());
  }
  void optional_title_fields_multiline_values_and_non_bmp_styles() {
    QTemporaryDir parent;
    const auto c = convert_fountain(intake("Authors:\n   **Ada**\n\tBob\nContact: Here\n   There\nCredit: Written by\nSource: Original\n\n!\xf0\x9f\x98\x80 **caf\xc3\xa9**\n\n. Not a forced heading\n\n.$also literal\n", parent.path()).assets);
    QCOMPARE(c.elements.size(), std::size_t(9));
    QCOMPARE(c.elements[0].element.type, ElementType::author);
    QCOMPARE(c.elements[0].element.text, std::string("Ada"));
    QCOMPARE(c.elements[0].element.styles, (std::vector<StyleSpan>{{0, 3, Style::bold}}));
    QCOMPARE(c.elements[1].element.text, std::string("Bob"));
    QCOMPARE(c.elements[2].element.type, ElementType::contact);
    QCOMPARE(c.elements[3].element.text, std::string("There"));
    QCOMPARE(c.elements[4].element.type, ElementType::credit);
    QCOMPARE(c.elements[5].element.type, ElementType::source);
    QCOMPARE(c.elements[6].element.styles, (std::vector<StyleSpan>{{5, 10, Style::bold}}));
    const auto saved = c.json().value("screenplay").toObject().value("elements").toArray()[0].toObject();
    QCOMPARE(saved.value("text").toString(), QString::fromUtf8("😀 café"));
    QCOMPARE(saved.value("styles").toArray(), (QJsonArray{QJsonObject{{"start", 2}, {"end", 6}, {"style", "bold"}}}));
    QCOMPARE(c.elements[7].element.type, ElementType::action);
    QCOMPARE(c.elements[7].element.text, std::string(". Not a forced heading"));
    QCOMPARE(c.elements[8].element.type, ElementType::action);
    QVERIFY(c.unresolved.empty());
  }
  void ambiguous_material_is_located_and_never_exportable() {
    QTemporaryDir parent;
    const auto c = convert_fountain(intake("!Cross *one _two* three_\n\n\\[[literal\n\n@23 (O.S.)\nText.\n\nA\nLeft.\n\nB ^  \nRight.\n\n.X #bad?#\n", parent.path()).assets);
    QVERIFY(codes(c.unresolved).contains("crossing-emphasis"));
    QVERIFY(codes(c.unresolved).contains("escaped-delimiter"));
    QVERIFY(codes(c.unresolved).contains("invalid-cue"));
    QVERIFY(codes(c.unresolved).contains("dual-trailing-space"));
    QVERIFY(codes(c.unresolved).contains("scene-number"));
    QCOMPARE(c.elements[0].element.text, std::string("Cross *one _two* three_"));
    QVERIFY(c.elements[0].element.styles.empty());
    for (const auto& issue : c.unresolved) QVERIFY(issue.range.end > issue.range.start);
    const auto bare = convert_fountain(intake("Unknown:\n\nVisible.", parent.path()).assets);
    QCOMPARE(codes(bare.unresolved), QStringList{"title-or-action"});
    QCOMPARE(bare.elements[0].element.text, std::string("Unknown:"));
    const auto large = convert_fountain(intake(QByteArray(1024 * 1024 + 1, 'x'), parent.path()).assets);
    QCOMPARE(codes(large.unresolved), QStringList{"conversion-limit"});
    QCOMPARE(large.unresolved[0].range.end, qsizetype(1024 * 1024 + 1));
    QVERIFY(large.elements.empty());
    const auto mixed = convert_fountain(intake("!**bold*\n\n!foo_bar_baz\n", parent.path()).assets);
    QCOMPARE(codes(mixed.unresolved), (QStringList{"mixed-emphasis", "intraword-emphasis"}));
    QCOMPARE(mixed.elements[0].element.text, std::string("**bold*"));
    QCOMPARE(mixed.elements[1].element.text, std::string("foo_bar_baz"));
    const auto empty_cue = convert_fountain(intake("!Before.\n\n@\nWords.\n", parent.path()).assets);
    QCOMPARE(empty_cue.unresolved[0].code, QString("invalid-cue"));
    QCOMPARE(empty_cue.unresolved[0].range.start, qsizetype(10));
    QCOMPARE(empty_cue.unresolved[0].range.end, qsizetype(11));
    QCOMPARE(empty_cue.unresolved[0].range.first_line, 3);
  }
  void reader_caps_only_display_and_invalid_encoding_never_uses_replacement_text() {
    QTemporaryDir parent;
    QByteArray many;
    for (int i = 0; i < 201; ++i) many += "[[note]]\n";
    auto captured = intake(many, parent.path());
    const auto interpreted = interpreted_fountain_assets(captured.assets);
    const auto reader = open_fountain_intake(interpreted);
    QCOMPARE(convert_fountain(interpreted).records.size(), std::size_t(201));
    QCOMPARE(reader.findings.size(), 200);
    QVERIFY(reader.summary.contains("capped at 200"));
    QCOMPARE(*reader.assets.original_fountain, many);
    captured = intake(QByteArray("\xff\xfe", 2), parent.path());
    const auto invalid = open_fountain_intake(interpreted_fountain_assets(captured.assets));
    QVERIFY(invalid.source_view.contains("ff fe"));
    QVERIFY(!invalid.findings[0].toMap().value("text").toString().contains(QChar(0xfffd)));
    const auto connected = convert_fountain(intake("[[first\r\n  \r\nlast]]\r\n\r\nVisible.", parent.path()).assets);
    QVERIFY(connected.unresolved.empty());
    QCOMPARE(connected.records[0].text, QString("first\r\n  \r\nlast"));
    const auto disconnected = convert_fountain(intake("[[first\r\n \r\nVisible.", parent.path()).assets);
    QCOMPARE(disconnected.unresolved[0].range.end, qsizetype(7));
    QCOMPARE(disconnected.elements.back().element.text, std::string("Visible."));
  }
  void official_retained_text_and_scene_numbers_are_independent_expectations() {
    const auto root = QStringLiteral(APOLLO_FIXTURE_ROOT "/fountain-syntax/");
    const auto notes = convert_fountain(read_fountain_intake(root + "Notes.fountain").assets);
    QCOMPARE(notes.records.size(), std::size_t(3));
    QCOMPARE(notes.records[0].text, QString("A note."));
    QCOMPARE(notes.records[1].text, QString("This line spans\n  multiple lines."));
    QCOMPARE(notes.records[2].text, QString("internal"));
    const auto stash = convert_fountain(read_fountain_intake(root + "Boneyard.fountain").assets);
    QCOMPARE(stash.records[0].text, QString(" This is an inline Boneyard. "));
    QCOMPARE(stash.records[1].text, QString(" \nThis is a\nmulti-line \nBoneyard. \n"));
    QCOMPARE(stash.records[2].text, QString(" internal "));
    const auto numbered = convert_fountain(read_fountain_intake(root + "SceneNumbers.fountain").assets);
    QStringList values;
    for (const auto& r : numbered.records) { values << r.text; QVERIFY(!r.anchor.isEmpty()); }
    QCOMPARE(values, (QStringList{"1", "1A", "1a", "A1", "I-1-A", "1.", "110A"}));
    const auto sections = convert_fountain(read_fountain_intake(root + "SectionHeaders.fountain").assets);
    QStringList names;
    for (const auto& r : sections.records) names << r.text;
    QCOMPARE(names, (QStringList{"Act One", "Act Two", "ACT THREE", "ACT FOUR", "5", "Level 2", "Level 3"}));
  }
  void hierarchy_production_associations_and_dual_boundaries() {
    QTemporaryDir parent;
    const auto c = convert_fountain(intake("# Act\n### Sequence\n= Section synopsis\n\n.int. home #1a#\n= Scene synopsis\n\nA\nLeft.\n\nB^\nRight.\n\n!Interruption.\n\nC ^\nUnpaired.\n", parent.path()).assets);
    QCOMPARE(c.records[1].parent, c.records[0].id);
    QCOMPARE(c.records[1].depth, 3);
    QCOMPARE(c.records[2].anchor, c.records[1].id);
    QCOMPARE(c.records[3].text, QString("1a"));
    QCOMPARE(c.records[3].anchor, QString::fromStdString(c.elements[0].element.id));
    QCOMPARE(c.records[4].anchor, c.records[3].anchor);
    QVERIFY(c.elements[3].element.dual_right);
    QVERIFY(!c.elements[6].element.dual_right);
    QCOMPARE(codes(c.unresolved), QStringList{"unpaired-dual"});
    for (const auto* cue : {"B^", "B ^", "B   ^"}) {
      const auto paired = convert_fountain(intake(QByteArray("A\nLeft.\n\n") + cue + "\nRight.", parent.path()).assets);
      QVERIFY(paired.unresolved.empty());
      QVERIFY(paired.elements[2].element.dual_right);
      QCOMPARE(paired.elements[2].element.text, std::string("B"));
    }
    const auto skipped = convert_fountain(intake("### Top\n## Other\n# Root\n## Child\n", parent.path()).assets);
    QVERIFY(skipped.records[0].parent.isEmpty());
    QVERIFY(skipped.records[1].parent.isEmpty());
    QVERIFY(skipped.records[2].parent.isEmpty());
    QCOMPARE(skipped.records[3].parent, skipped.records[2].id);
  }
  void migration_is_an_explicit_verified_copy_and_old_versions_still_open() {
    QTemporaryDir parent;
    const auto captured = intake("A source. [[note]]\n\n/* stash */\n", parent.path());
    const auto old = create_project(parent.filePath("old"), {0, {{ElementType::action, ""}}}, captured.assets);
    const auto old_manifest = bytes(old.directory + "/project.json"), old_metadata = bytes(old.directory + "/imported-material.json");
    const auto interpreted = interpreted_fountain_assets(open_project(old.directory).assets);
    const auto copy = create_project(parent.filePath("interpreted"), {0, {{ElementType::action, ""}}}, interpreted);
    QVERIFY(copy.import_review && copy.id != old.id);
    QCOMPARE(bytes(old.directory + "/project.json"), old_manifest);
    QCOMPARE(bytes(old.directory + "/imported-material.json"), old_metadata);
    QCOMPARE(open_project(old.directory).assets, captured.assets);
    QCOMPARE(open_project(copy.directory).assets, interpreted);
    QCOMPARE(*interpreted.original_fountain, *captured.assets.original_fountain);
    ProjectSession session(copy.directory);
    QVERIFY_THROWS_EXCEPTION(SourceError, session.save(serialize_screenplay({1, {{ElementType::action, "changed", false, {}, "a"}}})));
    const auto reopened = session.opened();
    RecoveryStore store(copy, interpreted, copy.directory + "/.recovery");
    const auto package = store.preserve(reopened.screenplay, "reload", QDateTime::currentDateTimeUtc());
    QCOMPARE(RecoveryStore::inspect_copy(package.directory)->opened.assets, interpreted);
    for (const auto* mutation : {"text", "range", "record", "permission", "revision", "unknown"}) {
      auto broken = interpreted;
      auto root = QJsonDocument::fromJson(*broken.imported_material).object();
      auto model = root.value("conversion").toObject();
      if (QString(mutation) == "text") model.insert("screenplay", QJsonObject{});
      if (QString(mutation) == "range") model.insert("locations", QJsonArray{});
      if (QString(mutation) == "record") model.insert("retained", QJsonArray{});
      if (QString(mutation) == "permission") model.insert("editable", true);
      if (QString(mutation) == "revision") model.insert("revision", 2);
      if (QString(mutation) == "unknown") model.insert("future", true);
      root.insert("conversion", model); broken.imported_material = QJsonDocument(root).toJson();
      QVERIFY_THROWS_EXCEPTION(SourceError, open_fountain_intake(broken));
    }
  }
};
QTEST_GUILESS_MAIN(FountainConversionTests)
#include "fountain_conversion_tests.moc"
