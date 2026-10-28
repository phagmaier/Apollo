# Editor/export spike progress

The spike is in progress. **Checks 1 (engine fixtures), 2 (editor/PDF
agreement), and 3 (PDF quality) pass for the exercised spike inputs**, as of
2026-10-27. Check 4 (interaction) has its Qt Test scripts but still needs the
owner's manual session under native Wayland with fcitx5. Checks 5–7
(responsiveness, code-shape review of the finished integration, look) remain
open; check 5 has only an early offscreen indicator. See
[spike-brief.md](spike-brief.md#passfail-checks).

**Done: PDF export (brief step 3) and the bounded QML `TextEdit` probe (step 4).
Step 5 is an unfinished checkpoint pending the owner's native session
(see [the editor section](#editor-brief-step-5)).** The
editor is stock QML `TextEdit` with Apollo's `ScreenplayLayout` and C++
commands for the brief's Enter, Tab, paste, undo, and scene-move rules.
Offscreen Qt tests cover those rules, caret and selection across page gaps and
through dual dialogue, and rendering; editor and PDF line dumps agree on every
fixture, both workloads, and the Academy transcription, before and after 50
scripted edits. **Next ready: the owner's manual session (instructions in the
editor section), then native measurements (check 5) and the look screen
(check 7).** No native-Wayland interaction, input-method, or performance
result is claimed from offscreen runs.

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

This section records the probe as it finished. Step 5 superseded parts of it:
finding 2 was incomplete (see [the editor's performance
work](#early-typing-latency)), and the stand-ins in finding 4 and the
limitations listed under "Not established" are resolved or re-recorded in the
editor section. Step 5 renamed the probe's files: `TextEditProbe.qml` is now
`ScreenplayEditor.qml`, `probe_view` is `editor_view`, `textedit_probe` is
`apollo_editor`, and `textedit_probe_tests` is `editor_tests`.

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

## Editor (brief step 5)

The editor builds on the probe: stock QML `TextEdit` (`editor/ScreenplayEditor.qml`)
shows the `QTextDocument`; `ScreenplayLayout` places every line at the engine's
breaks and point geometry; `EditorCommands` (`editor/editor_commands.cpp`)
applies the brief's rules through `QTextCursor`, one undo step per command.
QML intercepts Enter (with or without Shift), Tab, Shift+Tab, copy, cut, paste,
printable typing, and Alt+Shift+Up for the scene-move test command; everything
else (caret movement, selection, Backspace, Delete, undo, redo, input methods)
is `TextEdit`'s own. No Qt private header, copied Qt source, or private-class
subclass is used. The engine still builds and tests without QtGui.
`build/release/apollo_editor FILE` opens the editor.

### Engine rules for editor states

The probe's stand-ins are gone. The engine handles the incomplete editor
states exercised by the spike, so the editor and PDF paginate those states
the same way. Engine rejections still use the unpaginated editor fallback.
These are provisional spike rules, recorded in
[the profile](export-profile.md#editor-states-provisional-spike-rules) for owner
review: an empty element (no text, or only spaces) keeps one blank slot with
its normal spacing; a cue
without speech stands alone and moves whole; a right dual cue without a left
or right speech is an ordinary cue; parentheticals or dialogue without a cue
keep the speech spacing and move whole. Engine tests give hand-worked slots for
each, including the page-end cases, and failed before the rules existed.

Two engine defects found by the scripted edits are fixed, each with failing
tests first, without changing any fixture, workload, or Academy dump:

- **Long cues ran off the page.** A cue was always one line. A Backspace at the
  start of a dialogue merged it into its cue, and the PDF drew the 95-character
  cue past the page edge, where Poppler (like a printer) loses it. Cues now wrap
  at the profile's 36 columns (4.0 to 7.6 in), and their lines count toward fit
  and speech splits; a generated `(CONT’D)` may take a line of its own. Dual
  cues wrap at 20 columns (an Apollo proposal: the right cue's room to 7.6 in).
- **Trailing spaces moved aligned lines.** Enter after a space leaves one, and
  it counted toward a transition's or centered line's width, shifting it a
  column left. Spaces at a line end are no longer line text.

Review then found that an element of only spaces (a space typed into an empty
action) got no line at all, so its caret line could overlap the next element.
It now counts as empty; the regression test failed before the fix.

The transition rule (B6) used to lay the document out again from the first
element each time it moved an element; it now resumes from saved state. The
dumps are byte-identical, and an engine pass over the 444-scene workload went
from about 55 to 22 ms (with import and printing).

### Early typing latency

Measured first, before the commands, to find the performance risk early.
Method: `apollo_editor FILE --measure 200` opens the workload in the real QML
editor, puts the caret at the first character on the later page of a block that
crosses a page break after the middle of the document, and sends 200 synthetic
key presses (a repeating `the lamp turns and `), one every 60 ms after the
previous frame. Each is timed from dispatch to the next `frameSwapped`. This
covers Apollo and Qt Quick, not compositor or keyboard latency. Environment:
release build, Qt 6.12.0, GCC 16.2.1, Intel Core i5-9400 (6 cores), 11 GiB RAM,
`QT_QPA_PLATFORM=offscreen` with Qt Quick's software renderer (the tool prints
both). **This is not
check 5**, which requires native Wayland.

| Run | 202-page workload | 506-page workload |
| --- | --- | --- |
| Probe layout, first measurement | p50 146 ms, p95 149 ms; open 364 ms | not run |
| After the fixes below | p50 17.6 ms, p95 18.6 ms, max 21 ms, none > 100 ms; open 170 ms; PSS 49 MiB | p50 34.4 ms, p95 35.8 ms, max 36 ms, none > 100 ms; open 389 ms; PSS 87 MiB |

Phase timing (temporary instrumentation, removed) located the cost:

- About 85 ms per key was `TextEdit` rebuilding text nodes for all 202 pages.
  It renders only the visible region when large text arrives through its `text`
  property, but not for a document set with `setTextDocument`. Setting the
  public `QQuickItem::ItemObservesViewport` flag ourselves restores that; sync
  fell below 1 ms. A test scrolls a document over the large-text threshold and
  checks that every visible line is drawn.
- About 57 ms was `ScreenplayLayout` rebuilding every block's `QTextLine`s. It
  now rebuilds only blocks whose line breaks, relative positions, or added
  formats changed, and moves the rest; a test checks that the incremental
  layout equals a fresh layout after typing, Enter, Backspace, type change,
  paste, and undo, and that a keystroke that does not repaginate rebuilds one
  block. Decoration items update only changed rows.

What remains per key is linear in document size: reading the document into
elements, the engine pass, and checking every block (about 10 ms of the
202-page figure, 26 ms of the 506-page one). That is the next thing to reduce if
native measurements miss the target.

### Editing behavior

Implemented as the brief specifies, with these delegated details:

| Situation | Behavior |
| --- | --- |
| Enter at the end of an element | New element: dialogue after a cue or after a parenthetical led by a cue, otherwise action. Shift+Enter is Enter, so no line separator reaches the document. |
| Enter inside an element (including at its start) | Splits it into two elements of the same type; a dual flag stays with the first part. |
| Enter on an empty element | Changes it to action (already action: no change). |
| Tab / Shift+Tab | Cycles heading → action → character → parenthetical → dialogue → transition. Lyrics, centered text, title fields, and page breaks become action. |
| Cue, heading, or title typing | Shown uppercase through `QTextLayout` formats (ASCII letters, like the engine); the document keeps what was typed. |
| Plain-text paste | One action paragraph per non-empty line (`\r\n`, `\r`, `\n`, U+2028, U+2029 break lines; tabs become spaces), dialogue when pasted inside dialogue. The element at the caret splits around them; a single line joins it inline. |
| Apollo copy/paste | Copy also writes `application/x-apollo-elements` with types, emphasis, and dual flags, each element cut to the selection. A selection edge that covers none of a block leaves it out. A single element pastes inline when the destination has its type and dual flag; otherwise elements are inserted between the split halves of the element at the caret, preserving their block properties. Invalid Apollo metadata falls back to plain text. |
| Undo | Typing is grouped by word: a step starts at each word, after the caret moves, and after any other change. Enter, Tab, paste, cut, and scene move are one step each. |
| Scene move | Alt+Shift+Up moves the scene at the caret (heading through the element before the next heading) above the previous scene. |

Probe limitations, resolved for the spike:

- **Shift+Enter** is intercepted as Enter, and paste turns line separators into
  paragraphs, so U+2028 no longer reaches the layout.
- **Generated continuation text keeps its cue's emphasis**: decorations are
  styled runs, so a bold cue's `(CONT’D)` repeat is bold like the PDF, with a
  plain generated suffix.
- **A typed `(CONT'D)`** where G1 generates one is shown once, in the generated
  form, matching the profile's "never doubled". The typed suffix stays document
  text, drawn transparent beneath the generated suffix; the engine reports it
  as replaced source text. Without an engine layout (the unpaginated fallback
  after an engine rejection) nothing is hidden.
- **Title page and placeholders**: the title page is the editor's first page
  card, laid out by the engine like the PDF. A manual break's caret line goes
  two slots after the last line of the page it ends, or into the gap below a
  full page, never over text.

### Evidence

Interaction (`tests/editor_tests.cpp`, Qt Test, `offscreen`, software scene
graph, failing on any warning), on hand-worked fixture positions:

- Every fixture line, including fixture 07's title page, is at its hand-derived
  page, slot, and profile column; caret, hit-testing, typing, selection, copy,
  repagination, and preedit across the fixture 06 page gap still pass, and each
  keystroke's caret position is checked while typing at the boundary.
- Each Enter row of the brief's table; an empty element's own slot (the next
  cue moves from slot 6 to 8, with a rendered grab showing no overlap); Tab and
  Shift+Tab through the cycle, uppercase glyphs drawn for a heading and a typed
  cue, and type-change undo; a type change that repaginates redraws every
  moved line.
- Plain-text paste (paragraphs, line separators, tabs, dialogue, inline) with
  undo restoring the exact document; Apollo copy/paste keeping types and the
  italic, bold, and underline spans.
- Word-grouped undo and redo step by step, including after caret moves; scene
  move, undo, and redo restoring the exact elements.
- Down and Right/Left through fixture 08's dual block follow document order
  (left speech, right cue, right speech) at the profile columns; Shift+Down
  selects in that order, copies document-order text, and highlights the left
  speech and right cue but not the right speech.
- A new cue typed lowercase at the end of fixture 02 stays paginated while its
  speech is incomplete.

Deliberately broken implementations were caught by these tests: no word
grouping, no empty-element Enter rule, a stale layout cache, no hidden typed
suffix, no uppercase format, unstyled decoration runs, a reversed scene move,
Shift+Enter passed through, an empty action without its slot, and the wrong
Enter type after a cue. Dropping the caret-move check from typing grouping was
not caught, because `QTextDocument` never merges non-adjacent inserts anyway.

Editor/PDF agreement (`tests/editor_pdf_tests.py`, check 2): `apollo_editor FILE
--agreement DIR [EDITS]` loads the file in the real QML editor, optionally
applies scripted edits through key events, writes the editor's line dump, read
back from its laid-out `QTextLine`s and decorations (page, slot, element,
left edge, text), and exports the PDF of the same document. The test checks
every editor line's left edge against the profile within 0.01 pt, then requires
Poppler's reading of the PDF to match the dump exactly on page, slot, element
column, text, and page numbers. The 50 scripted edits are deterministic: ten
kinds (typing at an element end, Enter plus a new paragraph, splitting, merging
with Backspace, Tab, deleting a two-line selection, pasting two paragraphs,
scene move, undo, typing at an element start), five times each, each at an
element with a line in a page's last four or the next page's first two slots.

| Input | Pages (with title) | Lines | Later elements moved to another page during the 50 edits |
| --- | --- | --- | --- |
| Fixtures 01–08, unedited | 1–3 each | 251 in total | — |
| 175 scenes, seed 1: unedited / 50 edits | 203 / 204 | 9,226 / 9,273 | 4,737 |
| 444 scenes, seed 1: unedited / 50 edits | 507 / 509 | 23,034 / 23,090 | 19,329 |
| Local Academy transcription: unedited / 50 edits | 12 / 13 | 442 / 469 | 413 |

A stale-cache mutant passed the unedited comparison and failed after the
edits, so the edits exercise the editor's own state, not just the engine.
Agreement is not correctness on its own; the hand-derived fixture checks in
`editor_tests` and `pdf_tests` and the anchored Academy comparison are the
independent evidence.

### Checkpoint review

The complete uncommitted step-5 change was reviewed, including the staged
renames, unstaged edits, the new command files and PDF agreement test, and test
quality. Concrete issues fixed during review:

- A single copied Apollo element lost its type and dual flag when pasted into
  an empty or differently typed element. Paste now preserves those properties;
  Qt key-event tests cover both destinations, emphasis, and exact undo/redo.
- B2 did not count a wrapped cue's lines when it had no speech yet, leaving
  its heading on the preceding page. An oversized lone cue could also emit
  slots past 56. Hand-worked engine checks cover keeping the heading attached
  and rejecting an unsplittable cue longer than a page.
- B6 treated an unpaired right cue as dual when retrying a transition,
  rejecting an otherwise valid editor state. Retry now uses the actual dual
  pairing; a hand-worked page-end case checks that only the ordinary speech
  moves with the transition.
- Apollo clipboard JSON accepted invalid element types and unchecked style
  offsets, allowing out-of-bounds text reads. Metadata now requires valid
  types, spans within the text at UTF-8 boundaries, and one paragraph per
  element. Invalid metadata uses the plain-text fallback. Tests cover unknown
  types, negative and oversized spans, split UTF-8, and embedded newlines.

The paste, heading/oversized-cue, unpaired-cue retry, and invalid-type
regressions failed before their respective fixes. The additional invalid-span
cases exercise the same validation boundary without deliberately reading
outside the original text buffer. Fixture expectations, provisional product
rules, and the limitations below remain unchanged.

Checkpoint verification: `mise run check` and `mise run check-debug` each pass
all five CTest tests; `mise run check-engine` passes both tests. `editor_tests`
passes 46 Qt Test cases, including the new regressions. The independent
Academy comparator passes in release and sanitizer builds, and the release
PDF and editor/PDF suites pass with `--academy` (including 50 edits of the
transcription). Build-time font hashes pass; the engine-only executable has
no Qt library dependency, and the source uses no Qt private includes.
Sanitizers ran outside the tracing sandbox with no reported diagnostics;
the sandboxed checks used writable caches under `/tmp`. No native interaction
or new performance result is established by this verification.

### Not established

- **Native interaction (check 4)**: Wayland input, fcitx5 compose or dead keys,
  mouse drag selection, and the owner's manual session. Synthetic Qt Test keys
  do not go through the compositor or an input method.
- **Responsiveness, open time, and memory (check 5)** under native Wayland, and
  PDF export time while typing. The offscreen figures above are risk
  indicators only.
- **Product decisions** these rules stand in for: what Enter on an empty
  action should do, whether empty elements should appear in exports, whether a
  typed `(CONT'D)` should be normalized or kept as written (and the Fountain
  import rule for literals), plain-text paste paragraph rules, the Tab order
  for element types outside the brief's cycle, and Backspace merging a
  dialogue into its cue (allowed, now wrapped correctly; the spec's
  structural-problem flags would be the place to catch it).
- **Gaps**: typing into a manual-break element shows text the engine does not
  lay out (the break element holds no text in the PDF); only ASCII letters are
  shown uppercase in cues and headings, in both editor and PDF; the title page
  is edited in place with no field-specific controls; `ScreenplayLayout::draw`
  is unused by `TextEdit` and untested; a cue repeated after a split is drawn
  by QML `Text`, which the ink checks cover but glyph-level comparison does not.

### Manual session (owner)

Run these from a terminal inside your Hyprland session, so the editor is a
native Wayland window and inherits Omarchy's `QT_IM_MODULE=fcitx`.

```sh
cd ~/Code/apollo
mise run check                       # builds build/release and regenerates the workloads
echo "$QT_IM_MODULE"                 # should print: fcitx
pgrep -a fcitx5                      # fcitx5 must be running
QT_QPA_PLATFORM=wayland build/release/apollo_editor build/release/editor-pdf-tests/workload-175.fountain
```

In the window (a page boundary is wherever a white page card ends):

1. Click at the start of the first line on any page after page 2 and type a
   sentence. The caret should stay right after each character as text moves
   between pages; nothing should overlap.
2. Shift+Down across a page gap, Ctrl+C, then paste into another terminal
   window to check the copied text. Then drag-select across a gap with the mouse.
3. Find a dual-dialogue block (side-by-side speeches), click in the left
   speech, and press Down repeatedly: the caret should go through the left
   speech, then the right cue and speech. Shift+Down should highlight in that order.
4. Enter at the end of a cue (new dialogue), Enter at the end of action (new
   action), Enter in the middle of a line (split), Enter on an empty line (it
   becomes action); Tab and Shift+Tab on a line to cycle its type; type a cue in
   lowercase and check it shows uppercase.
5. Copy a cue and its dialogue and paste them elsewhere (types and emphasis
   kept); copy plain text from another app and paste it (action paragraphs).
6. Ctrl+Z after typing (word by word), after Tab, and after Alt+Shift+Up (scene
   move); Ctrl+Shift+Z to redo.
7. Compose: press Caps Lock (Omarchy's Compose key), then `'`, then `e`. An `é`
   should appear once at the caret, with no stray characters or caret jump,
   including when done at the start of a page. For a dead key, add "English
   (US, intl., with dead keys)" in `fcitx5-configtool`, switch to it, type `'`
   then `e`, and switch back afterwards.

Then, still in the Hyprland session, record native timings (the window must
stay visible and focused while it types for about 20 seconds):

```sh
QT_QPA_PLATFORM=wayland build/release/apollo_editor build/release/editor-pdf-tests/workload-175.fountain --measure 200
QT_QPA_PLATFORM=wayland build/release/apollo_editor build/release/editor-pdf-tests/workload-444.fountain --measure 200
```

Note anything that felt wrong (with the page and what you typed), the compose
result, and the printed p50/p95/max, open time, and PSS lines.

### Reproduction

```sh
mise run check            # includes editor_tests and editor_pdf_tests
mise run check-debug
mise run check-engine
QT_QPA_PLATFORM=offscreen uv run tests/editor_pdf_tests.py --editor build/release/apollo_editor --output build/release/editor-pdf-tests --academy
QT_QPA_PLATFORM=offscreen build/release/apollo_editor build/release/editor-pdf-tests/workload-175.fountain --measure 200
build/release/apollo_editor build/release/editor-pdf-tests/workload-175.fountain --check
```

## Remaining work and next step

The next concrete step is the owner's manual session and native timings
(see [Manual session](#manual-session-owner)), then the rest of check 5 (open
time and PDF export while typing, under native Wayland) and the look screen
(check 7, brief step 6). If native typing misses the target, reduce the
remaining per-key work in `ScreenplayLayout` (reading the document and
checking every block) before considering the custom `QQuickItem` fallback. The
engine remains independent of QtGui.

Title-page overflow/long fields, unsupported or malformed Fountain, emphasis
beyond the exercised cases, multi-element lyric grouping, broader heading and
transition boundary combinations, and large-document performance still need
verification. The spike exporter currently rejects unavailable glyphs instead
of substituting fonts; the product's unexpected-character policy is still open.
The CLI uses atomic replacement and reports failure, while application export
UX, asynchronous export, and final metadata policy remain later work.

The full spike still requires native interaction and performance checks, the
QML visual screen, the owner's manual session, code-shape review of the
completed integration, and a final keep/switch recommendation. No export
latency, native-Wayland responsiveness, or native input-method result is
claimed.
