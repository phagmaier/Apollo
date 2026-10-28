# Editor/export spike progress

The spike is in progress. **Acceptance check 1 (engine fixtures) and check 3
(PDF quality) pass for the exercised spike inputs**, as of 2026-10-27. Checks
2, 4–7 in [spike-brief.md](spike-brief.md#passfail-checks) remain incomplete.

**Done: PDF export (brief step 3).** The engine carries shared point geometry
and page decorations. The minimal PDF painter passes independent fixture and
font/geometry checks, plus engine/PDF agreement on both generated workloads and
the local Academy transcription. **Next ready: the bounded QML `TextEdit` probe
(brief step 4).** No Qt editor, editor/PDF agreement, or interactive export
performance result exists yet.

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

## Remaining work and next step

The next concrete step is the short, fixed-time QML `TextEdit` probe with a
custom `QAbstractTextDocumentLayout`, recording whether public APIs support the
engine's real page geometry and editing behavior before building the editor
item. Then check editor/PDF agreement through scripted edits and the specified
interaction cases. The engine remains independent of QtGui.

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
