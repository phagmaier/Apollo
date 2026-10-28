# Editor/export spike progress

The spike is in progress. **Acceptance check 1 (engine fixtures) and check 3
(PDF quality) pass for the exercised spike inputs**, as of 2026-10-27. Checks
2, 4–7 in [spike-brief.md](spike-brief.md#passfail-checks) remain incomplete.

**Done: PDF export (brief step 3)** and **the bounded QML `TextEdit` probe
(brief step 4, 2026-10-27).** The probe shows that stock `TextEdit`, given a
`QTextDocument` with Apollo's custom layout through public APIs, renders,
hit-tests, moves the caret, selects, copies, and reflows across a real page gap
in offscreen tests (see [the probe section](#qml-textedit-probe-brief-step-4)).
**Next ready: the editor (brief step 5), starting from stock `TextEdit`, then
checks 2 and 4.** No editor/PDF line-dump agreement, native-Wayland
interaction, input-method, or performance result exists yet.

All eight hand-derived layout fixtures and their PDFs match the unchanged
expected page, slot, element columns, text, and exercised style annotations.
All 11 Academy body pages match at the engine level after page anchoring and
only the four documented deviations. These results do not establish complete
profile/importer conformance or performance.

- CMake/Ninja release and sanitizer presets, mise tasks, and the four pinned
  Courier Prime fonts with build-time hash checks are in place.
- The Qt-independent layout library and importer cover the fixture constructs,
  including emphasis spans, lyrics, centered text, transitions, title fields,
  speech and action splits, continuation cues, and dual dialogue. Tests compare
  all eight fixtures end to end through `import_fountain` and `layout`. The
  importer is a spike subset, not a full Fountain compatibility implementation.
- Regenerated `--scenes 175 --seed 1` and `--scenes 444 --seed 1` inputs import and
  lay out without an error: 202 and 506 body pages respectively. These counts are
  observations, not independent proofs of page correctness or performance. Use
  `mise exec -- cmake --build --preset release`, then
  `build/release/layout_probe INPUT.fountain > OUTPUT.tsv` to repeat the dump.
- The Academy comparison is now repeatable with `tools/compare_academy.py` (see
  below). It checks the local TSV against the PDF's measured rows and the
  Fountain transcription against those rows before comparing engine output.
- Release, sanitizer, and engine-only tests pass. The Academy reference and
  Academy engine/PDF comparisons also pass in release and sanitizer builds.
  Sanitizer execution ran outside the tracing sandbox so LeakSanitizer could
  run; no sanitizer diagnostics were reported. Qt remains 6.12.0 and GCC
  16.2.1; no tool/font pins changed.

## Independent Academy verification

The comparison follows the fixture guide: restart from each reference page's
first line, retain all later source text, and compare the resulting first page.
Keeping the suffix rather than truncating at the page end exercises the real
split/push decision. The continuation-page suffix includes its cue. The tool
also checks the full unanchored document's 245 body elements for exact
paragraph/element text preservation, excluding generated page continuations and
rewrapping. It never rewrites the reference files.

The earlier deviation descriptions used PDF page numbers, including the title;
the TSV numbers body pages. The fixture guide now labels both explicitly. All
numbers below are **body pages**.

| Reference body page | Result |
| --- | --- |
| 1–3 | Exact page/slot/element/text match: 39, 39, and 44 lines |
| 4 | 42 lines match with the continuation cue at slot 1 instead of margin slot 0; following slots +1 |
| 5 | All 41 reference lines match; the next three-line speech fits in slots 54–56, as documented |
| 6 | 37 lines match; automatic spacing after the centered date shifts subsequent slots −2 |
| 7 | All 36 reference lines match; the next Shot-as-action line fits at slot 56, as documented |
| 8–11 | Exact match: 36, 41, 44, and 37 lines |

One genuine discrepancy remained after anchoring: the final centered line was
at slot 46 rather than 47. The local PDF confirms two blank slots after the
transition at slot 44. The profile's blanket one-blank rule was too broad;
the profile and engine now use two before centered text immediately following
a transition. This is a measured rule correction, not a fifth allowed deviation.
The regression failed before the fix and passes afterward. Boundary coverage
also checks that the extra spacing counts toward fit and drops at a page top.

The boundary review reproduced a separate B2 defect: a heading could remain at
slot 54 while a following three-line centered or lyric element moved to page 2.
The heading now moves with either unsplittable element. Both regressions failed
before the fix; companion cases confirm that a one-line element still fits
exactly at slot 56. Existing action, speech, dual-dialogue, parenthetical, and
transition tests continue to pass. No small-fixture expectations changed.

In the unanchored full run, keeping the three-line speech on body page 5 starts
the next reference page's remaining content four slots earlier. Omitting the
two extra blanks after the date makes later content six slots earlier. This
allows the next speech to start at slot 52 on Apollo page 6: two dialogue lines
at 53–54, `(MORE)` at 55, and eight rewrapped dialogue lines after the cue on
page 7. That is the latest qualifying B3 sentence split, not an unexplained
defect or an additional reference exception. The full run has 11 body pages
and preserves all source text; its later page boundaries are not expected to
match the reference without anchoring.

Reproduction (Poppler and the three local `reference/academy/` files required):

```sh
mise run check
uv run tools/compare_academy.py --probe build/release/layout_probe
mise run check-debug
uv run tools/compare_academy.py --probe build/debug/layout_probe
```

Both check tasks passed, as did both Academy comparisons. Before the spacing
fix, the comparator failed specifically on body page 11. The independent local
PDF used here has SHA-256
`650d0a9a3b24048f761cf05f1c7f5873a3f74a8bb926d23de700c8fbd51993ba`.
The PDF, transcription, expected TSV, and diagnostic dumps remain local;
no Academy screenplay text was added to the tests.

## PDF export verification

`layout_document` now supplies point positions and natural-advance widths for
every line, with explicit pages and page-number geometry. It covers hanging
parentheticals, dual columns, transition/center/title alignment, contact/date
fields, and page labels. Explicit page metadata preserves empty pages from
consecutive or trailing manual breaks; title inclusion can be disabled without
changing body numbering. The existing `layout` line view remains available.

The painter uses public Qt APIs: registered bundled fonts, `QRawFont::fromFont`,
[`QGlyphRun`](https://doc.qt.io/qt-6/qglyphrun.html) at the engine's glyph
origins, and [`QPdfWriter`](https://doc.qt.io/qt-6/qpdfwriter.html). It paints
the selected regular/italic/bold/bold-italic faces and draws underlines using
the selected font's `post` metrics. It does not paginate or wrap text again.
An early unregistered raw-font attempt produced visible outlines with no
extractable text; the PDF regression detected that. Registering the bundled
faces and deriving the raw fonts from those registrations produces embedded
TrueType fonts with Unicode mapping and extractable text.

Independent checks in `tests/pdf_tests.py` inspect actual output with Poppler:

- All eight fixture PDFs match their hand-derived text, pages, slots, and
  element columns, including dual columns and the side-by-side title fields.
  No fixture expectation was changed.
- `pdfinfo` reports 612×792 pt Letter pages. `pdffonts` reports only embedded
  Courier Prime faces with Unicode mapping. The mixed-style case exercises all
  four faces; `pdftohtml` confirms 12 pt text and bold/italic annotations.
- Left and right text edges agree with the profile to within 0.01 in. The body
  grid is checked using the profile baseline and the pinned font's descent,
  not the Academy PDF's unrelated text-box calibration. A 144 dpi raster check
  verifies the fixture underline's stroke and that it stops at the marked word.
- Additional checks cover title exclusion, mixed emphasis inside a word,
  combined bold/italic, accented/curly-quote text, blank manual-break pages,
  rejection of unavailable glyphs, destination errors, and prevention of input
  overwrite. A failed export leaves an existing destination unchanged.

The generated-input comparisons are **agreement checks**, supplemented by the
independent small fixtures and Academy evidence above:

| Input | Body pages | PDF lines matching engine page/slot/text and profile columns |
| --- | --- | --- |
| 175 scenes, seed 1 | 202 | 9,222 |
| 444 scenes, seed 1 | 506 | 23,030 |
| Local Academy transcription | 11 | 439 |

Workload and Academy agreement runs omit title fields; title inclusion and
exclusion are separately checked with fixture 07. Academy raw page boundaries
retain the documented cumulative shifts explained above. Agreement of its
full PDF with the engine does not replace the independent anchored comparison.
Title and dual-dialogue fixture PDFs were also rasterized and visually inspected.
This is not the owner's QML look acceptance.

The geometry tests first failed on missing point placement, then passed against
independently worked numeric expectations. The first PDF regression failed on
missing screenplay text, and also caught the outlined-font attempt. Review
covered the engine/PDF boundary, page/style handling, failure behavior, and
test independence. Release and sanitizer suites pass all three tests
(`layout_tests`, `geometry_tests`, `pdf_tests`); the engine-only build passes its
two tests and its executable has no Qt library dependency. The implementation
uses no Qt private headers or copied Qt source.

Reproduction:

```sh
mise run check
mise run check-debug
mise run check-engine
QT_QPA_PLATFORM=offscreen uv run tests/pdf_tests.py --exporter build/release/pdf_export --probe build/release/layout_probe --output build/release/pdf-tests --academy
QT_QPA_PLATFORM=offscreen uv run tests/pdf_tests.py --exporter build/debug/pdf_export --probe build/debug/layout_probe --output build/debug/pdf-tests --academy
```

Artifacts stay in the ignored build directories. The ordinary suite regenerates
its workloads there and does not need copyrighted reference files; `--academy`
requires the local transcription. See the fixture guide for individual export
and PDF line-dump commands. Test duration is not an export-performance or
responsiveness measurement.

## QML TextEdit probe (brief step 4)

**Question.** Can stock QML `TextEdit` show a `QTextDocument` laid out by an
Apollo `QAbstractTextDocumentLayout` at the engine's breaks and point geometry,
and keep caret, hit-testing, selection, and typing correct across a real page
gap, using only public Qt 6.12.0 APIs? **Timebox:** four hours, set at
2026-10-28T01:18Z; the probe and its record finished in under one hour.

**Result: yes, for the exercised offscreen cases.** No Qt private headers,
copied Qt source, or subclassing of Qt private classes is used. Qt 6.12.0's
public `qquicktextedit.cpp` and `qquicktextcontrol.cpp` were read to choose
hypotheses; the tests, not that reading, are the evidence.

How it works (`editor/`):

- One `QTextBlock` per element, with the element type and dual flag as block
  properties and emphasis as character formats. `document_elements` turns the
  live document back into engine elements; a round-trip test covers all eight
  fixtures, including per-byte emphasis.
- `ScreenplayLayout` runs `layout_document` on the whole document after every
  change. It creates each block's `QTextLine`s with `setNumColumns` at the
  engine's breaks (wrap-anywhere, so a split inside a long word also matches)
  and `setPosition` at the engine's x and baseline, offset by the page. A block
  split across pages is one block whose lines straddle the page gap.
- The engine now reports each line's **source mapping**: input element index,
  UTF-8 byte offset, and the length of document text in the line. Generated
  lines (`(MORE)`, a cue repeated after a split) have no source, and an
  automatic ` (CONT’D)` is a generated suffix. Hand-counted offsets for speech
  splits, interrupted and dual cues, dual parentheticals, and the transition
  retry are tested, as is that every non-space source byte is mapped exactly
  once in all eight fixtures. Mutating the split base, the retry index shift,
  or the dual part index each failed these tests.
- Generated text and page numbers are `decorations` drawn by QML `Text` items
  over the page cards (`editor/TextEditProbe.qml`); they are never document
  text. `TextEdit` cannot draw page cards or generated text itself.
- `QQuickTextDocument::setTextDocument` (public since 6.7) hands the document
  to `TextEdit`. The layout must be installed first: `TextEdit`'s text control
  connects to the document's layout at that moment.

Findings that shape the editor:

1. **Document units are 96 dpi pixels (4/3 px per pt).** `QTextLayout` keeps
   advances and positions in 1/64 px. At 12 px per 12 pt line, the Courier
   Prime advance 7.1953125 became 7.1875 (0.48 pt short across 61 columns) and
   x = 403.2 became 403.188. At 16 px the advance 9.59375 is exact in all four
   faces; line x positions are rounded to the 1/64 px grid (at most 0.006 pt off).
2. **`TextEdit` rebuilds every text node on each text change.** Its
   `q_textChanged` marks the whole document dirty, so blocks moved by
   repagination are redrawn correctly; the suspected stale-node defect did not
   occur and no workaround is used. This costs work in proportion to the
   rendered document per keystroke; above 10,000 characters `TextEdit` limits
   rendering to the viewport. Combined with whole-document relayout, this is
   the main performance risk for check 5 and is unmeasured.
3. **Input-method preedit** lives in the block's `QTextLayout` text, not the
   document. The layout offsets the engine's columns at the preedit position.
   Without that, a composing character broke the mapping; the regression test
   failed first. Preedit text is not rewrapped until it is committed.
4. **In-progress documents.** The engine rejects some states the editor will
   pass through, such as a new cue with no dialogue yet. The layout then falls
   back to an unpaginated stack so editing continues, and pagination returns
   once the speech is complete. The engine also gives empty elements no slot,
   and the probe does not place title-page elements; it puts their caret line
   two slots after the previous line, which can overlap the next element
   (title fields overlap page 1's first lines). Step 5 needs engine rules for empty and
   incomplete elements rather than these stand-ins.

Evidence (`tests/textedit_probe_tests.cpp`, Qt Test, `offscreen`, software
scene graph, failing on any warning):

- Every line and generated label of fixtures 01–06 and 08 appears at its
  hand-derived page and slot, profile column, and page-number position within
  0.01 pt; `(MORE)` and `CONT’D` are absent from document text. Fixture 07's
  title page is not edited by the probe. Glyph positions inside each line of
  fixture 01, including italic, bold, and underlined runs, step by the
  profile advance.
- `positionToRectangle` and `positionAt` map positions on both sides of the
  fixture 06 action split; presses in the page gap or blank slots go to the
  nearest line. Right, Left, Down, Up, Home, and End cross the gap in document
  order.
- Typing `Lightning. ` at the start of page 2 keeps the caret immediately after
  each typed character, ends with the hand-derived split after `Lightning.`,
  and one undo restores the original text and page.
- Shift+Down twice selects across the gap; `selectedText` and the clipboard
  match the expected text, and a rendered grab shows highlight on both pages
  and none in the gap.
- Lengthening an early action moves the split to after `chair.` with `Theo` at
  page 2 slot 1. Grabs before and after show ink only inside the current line
  and label rectangles and ink in every non-empty one.
- A preedit `´` then a committed `é` at the gap keeps the mapping and caret.
- A cue without dialogue keeps every block editable and recovers its pages.

Release and sanitizer builds pass all four test programs (`layout_tests`,
`geometry_tests`, `pdf_tests`, `textedit_probe_tests`); the engine-only build
passes its two tests without Qt. Both Academy comparisons and both PDF
`--academy` runs still pass. `build/release/textedit_probe FILE --check` laid
out the regenerated 175- and 444-scene workloads (202 and 506 body pages) and
the local Academy transcription with every block's lines at the engine's
breaks, in release and sanitizer builds. That is a structural mapping check,
not a timing result. Grabs are saved in `build/*/textedit-probe/`.

**Not established by the probe:** editor/PDF line-dump agreement and the 50
scripted edits (check 2); Enter/Tab element rules, paste kinds, undo grouping
of typing, type changes, and scene moves; caret and selection through dual
dialogue (document order is block order, untested); mouse drag selection;
native Wayland, fcitx5 dead keys or compose; responsiveness, open time, and
memory (check 5); the QML look (check 7). `ScreenplayLayout::draw`, the
`QPainter` path, is unused by `TextEdit` and untested. `TextEdit`'s own Enter
copies the current block's type; Apollo's element rules must intercept keys.
Shift+Enter inserts U+2028, which `QTextLayout` always breaks at but the engine
wraps as an ordinary character, so the layout reports a break mismatch.
Decorations are drawn unstyled, so a bold or underlined cue repeated after a
split is plain in the editor but styled in the PDF. A typed straight-apostrophe
`(CONT'D)` that the engine replaces with the generated form is still document
text and is drawn under the generated suffix; the import rule for literal
CONT'D belongs to the Fountain mapping work.

**Recommendation.** Build the step-5 editor on stock `TextEdit` with
`ScreenplayLayout`, intercepting Enter, Tab, and paste in QML/C++ commands over
`QTextCursor`, rather than writing a custom editor item first. Measure
keystroke latency at a page boundary on the 200-page workload early in step 5,
because of findings 2 and 4. Fall back to a custom `QQuickItem` that paints
the same `QTextLayout` lines if that measurement, input methods under native
Wayland, or dual-dialogue caret order fail and cannot be fixed through public
`TextEdit` and `QTextDocument` APIs. No stack switch is indicated by the probe.

Reproduction:

```sh
mise run check
mise run check-debug
mise run check-engine
build/release/textedit_probe build/release/pdf-tests/workload-175.fountain --check
build/release/textedit_probe fixtures/layout/02-dialogue-split.fountain  # interactive window
```

## Remaining work and next step

The next concrete step is the editor (brief step 5) on stock `TextEdit` and
`ScreenplayLayout`: engine rules for empty and incomplete elements, Apollo's
Enter/Tab/paste/undo behavior, an editor line dump for editor/PDF agreement
through scripted edits, and the interaction cases, with an early latency
measurement. The engine remains independent of QtGui.

Title-page overflow/long fields, unsupported or malformed Fountain, emphasis
beyond the exercised cases, multi-element lyric grouping, broader heading and
transition boundary combinations, and large-document performance still need
verification. The spike exporter currently rejects unavailable glyphs instead
of substituting fonts; the product's unexpected-character policy is still open.
The CLI uses atomic replacement and reports failure, while application export
UX, asynchronous export, and final metadata policy remain later work.

The full spike still requires editor/PDF agreement, interaction and performance
checks, the QML visual screen, the owner's manual session, code shape review of
the completed integration, and a final keep/switch recommendation. No export
latency, memory, native-Wayland responsiveness, or editor result is claimed.
