# Apollo test fixtures

These fixtures check Apollo's layout, export, import, and performance against expectations that do not come from Apollo's own engine. The profile they assume is [docs/export-profile.md](../docs/export-profile.md) (Academy sample profile v1).

## Layout fixtures — `layout/`

Eight small original scripts, each a `.fountain` file paired with an `.expected` file. The expected files were derived by hand from the profile rules and use original text only. Their line wrapping was cross-checked with `tools/afterwriting-oracle.sh`.

| Fixture | Checks |
| --- | --- |
| `01-element-geometry` | Every element's wrap width, hanging parenthetical, cue extension, emphasis that does not count toward width, lyric, transition, centered text, heading spacing |
| `02-dialogue-split` | Speech split at a mid-line sentence end, `(MORE)`, `NAME (V.O.) (CONT’D)`, continuation rewrapped |
| `03-heading-keep-with-next` | Heading alone at the bottom; heading whose unsplittable action follows |
| `04-dialogue-push` | Speech moves whole when a split would leave one line |
| `05-parenthetical-at-break` | Speech moves whole rather than leaving a parenthetical behind |
| `06-action-split` | Action split at the latest sentence end leaving two lines on each page |
| `07-title-page-and-manual-break` | Title-page placement, title page excluded from numbering, `===` break, body page 1 unnumbered |
| `08-dual-dialogue-and-contd` | Generated `(CONT’D)` after action, none after another speaker, dual columns, dual block moved whole |

`.expected` format: a line `page <n> number=<text|none>` starts each page (the title page is `page title`). Each following line is `slot<TAB>element<TAB>text[<TAB>styles]`, with blank slots omitted. Slot 1 is the first body line, 1.0 in from the top. Element x positions come from the profile's geometry table, so the files do not depend on the pending dialogue-column choice. Dual-dialogue lines carry `-L`/`-R`, and title-page lines carry their alignment.

How Apollo should pass:

1. Export each fixture to PDF.
2. Run `uv run tools/pdf_lines.py out.pdf` to dump its lines.
3. Map each line's x position to an element through the profile.
4. Require an exact match on page, slot, element, and text.

The spike also compares the editor's own line dump with the same expectations:
`tests/editor_tests.cpp` places every fixture line in the offscreen editor, and
`tests/editor_pdf_tests.py` requires the editor's dump to equal each fixture
PDF's lines (see [spike progress](../docs/spike-progress.md#editor-brief-step-5)).

### Current PDF checks

`mise run check` and `mise run check-debug` now export all eight fixtures and
inspect their actual PDFs with Poppler. The checks compare text, page, slot, and
profile columns; require Letter size and embedded Courier Prime with Unicode
mapping; inspect bold/italic text and a rasterized underline; and cover title
exclusion, mixed faces/Unicode, blank manual-break pages, and export failures.
Both generated workloads are recreated in the build directory and their PDF
lines compared with the engine. The editor/PDF checks run separately, in
`build/*/editor-pdf-tests/`. The `.expected` files remain unchanged.

The editor's asynchronous export also runs all eight independent fixture
checks after replacing/undoing/redoing the live text following the request.
Those PDFs must still match the original hand-derived pages, columns, text,
emphasis, embedded fonts, and title option. Export lifecycle/failure checks are
in `export_job_tests`; native typing/export measurements are included in
`tools/check-wayland.sh`. See the progress document for method and limitations.

Artifacts are in `build/release/pdf-tests/` (or `build/debug/pdf-tests/`). To
export or inspect one directly:

```sh
QT_QPA_PLATFORM=offscreen build/release/pdf_export fixtures/layout/01-element-geometry.fountain build/release/example.pdf
uv run tools/pdf_lines.py build/release/example.pdf --first-line-ymax 85.1 --column-gap 10.8 --number-band 60
```

Add `--no-title-page` to `pdf_export` to omit title fields without changing body
numbering. Apollo's first-slot `ymax` is **85.1 pt**, from the profile's 81 pt
baseline and the pinned font's 700/2048 em descent at 12 pt. Do not reuse the
Academy PDF's 70.8 pt calibration for Apollo. `--column-gap 10.8` preserves dual
columns and side-by-side title fields; default row grouping remains unchanged
for the Academy measurement tool.

For local Academy engine/PDF agreement as well as the ordinary PDF suite, run:

```sh
QT_QPA_PLATFORM=offscreen uv run tests/pdf_tests.py --exporter build/release/pdf_export --probe build/release/layout_probe --output build/release/pdf-tests --academy
```

This last agreement check supplements the independent page-anchored reference
comparison below; agreement with the engine alone does not prove correctness.

### What the afterwriting cross-check does and does not confirm

[afterwriting](https://github.com/ifrost/afterwriting-labs) (MIT) is an independent Fountain-to-PDF formatter. `tools/afterwriting-oracle.sh` installs version 1.17.3 into `~/.cache/apollo/afterwriting` with install scripts denied, and patches its US Letter profile to the Academy widths: 56 lines, dialogue 35, parenthetical 17. Checked on 2026-10-27:

- It reproduces 345 of 349 Academy body lines, and every expected line in fixtures 03, 04, 05, and 07.
- It differs by design where Apollo's profile follows the Academy rather than afterwriting:
  - it counts emphasis markers toward width (fixture 01);
  - it splits speeches at any line rather than at sentence ends (02 and 06);
  - it does not generate `(CONT’D)` after action, and uses its own dual-dialogue widths (08);
  - it puts one blank line after `FADE IN:` before a heading.
- Use it to catch wrapping mistakes and to estimate page counts. It is not a page-break authority.

## Source format — `source/`

`v1-example.screenplay.json` is a hand-written screenplay file in Apollo's
saved format ([source-format.md](../docs/source-format.md)), covering every
element type, title fields, deliberate line breaks, dual dialogue, and styles
over non-ASCII and non-BMP text. Its style offsets count code points; the
matching in-memory UTF-8 byte offsets are worked out by hand in
`tests/source_tests.cpp`. `source_tests` requires Apollo's output for that
screenplay to equal this file byte for byte. Edit both together.

## Academy reference — `../reference/academy/` (local only, gitignored)

This holds Final Draft's real output for the 11 body pages of the Academy guide. The guide's text belongs to the Academy, so it stays outside version control.

1. Download the [format guide PDF](https://www.oscars.org/sites/oscars/files/scriptsample.pdf) in a browser; the site blocks scripted downloads.
2. Save it as `reference/academy/scriptsample.pdf`.
3. Run `uv run tools/academy_reference.py`. It writes `expected.tsv`, every line with page, slot, and element, and `academy.fountain`, a transcription.

A copy was saved during planning on 2026-10-27.

Apollo should reproduce every line's text and slot, page by page, with these expected deviations:

- **PDF page 5 / body page 4 top:** Final Draft draws the `(CONT’D)` cue above slot 1; Apollo puts it on slot 1, shifting this page's remaining slots by +1.
- **PDF pages 6 → 7 / body pages 5 → 6:** Final Draft breaks one speech early for no rule-based reason; Apollo keeps `JOE / How did we reach…` in body page 5, slots 54–56.
- **PDF page 7 / body page 6:** the guide's extra blank lines after the centered date have no Apollo equivalent yet. Automatic spacing shifts the following slots by −2.
- **PDF pages 8 → 9 / body pages 7 → 8:** `APRIL` is a Final Draft "Shot" line; as Apollo action it fits in body page 7, slot 56.

Because these deviations shift later content, compare each page starting from the reference page's first line.

The repeatable engine-only comparison is:

```sh
mise run check
uv run tools/compare_academy.py --probe build/release/layout_probe
```

This requires the three local Academy files and Poppler. The tool checks that
the TSV still matches the PDF measurement and the Fountain file still matches
the transcription. It lays out each reference page's suffix from its first
line, retaining all subsequent text so page-end split/push decisions are tested.
The continuation-page suffix starts with its continuation cue. It compares every
first-page slot, element, and text after applying only the four changes above;
body pages 1–3 and 8–11 have no allowances. It also checks the full unanchored
document's paragraph/element text sequence, ignoring generated page continuations
and rewrapping. A mismatch returns a nonzero exit status; reference files are
never rewritten. Copyrighted inputs and temporary outputs remain local.

This verifies engine lines, not Apollo PDF geometry or editor/PDF agreement.
The Academy's final centered line follows a transition with two blank slots;
that measured rule is part of the profile, not an additional deviation.

## Fountain syntax samples — `fountain-syntax/`

These are the official Fountain parser test files from [nyousefi/Fountain](https://github.com/nyousefi/Fountain) (MIT; see `fountain-syntax/LICENSE`). Each one covers one construct: boneyard, notes, dual dialogue, forced elements, sections, synopses, scene numbers, page breaks, transitions, centered text, and indenting. Use them for import coverage and for the unsupported-construct report. They have no expected layouts. Task 2.6a exercises all 16 files for byte-exact
intake/reopen; this is not parser or layout acceptance. The
[compatibility table](../docs/fountain-compatibility.md) records each file and
the remaining semantic contract. Original independent examples and hand-worked
conversion expectations live in [import/README.md](import/README.md).
Task 2.6b now compares complete type/text/dual interpretations of all 16 samples
against hand-worked expected sequences in `import/interpretations.expected.json`,
including explicit unresolved interpretations. These do not establish editable
round-trip compatibility or independent layout correctness.

The same repository includes *Big Fish* (John August), a real full-length screenplay. Keep it in `reference/big-fish.fountain` as a local realistic import and performance input; it is gitignored. It renders to 124 pages with the oracle.

## Generated workloads — `generated/` (create on demand)

`tools/generate_script.py` writes deterministic synthetic scripts from an original word list. The same arguments always produce the same bytes. Each script has every element type, including dual dialogue, lyrics, transitions, centered text, and manual breaks.

```sh
uv run tools/generate_script.py --scenes 175 --seed 1 > fixtures/generated/feature-200.fountain   # 201 pages (oracle)
uv run tools/generate_script.py --scenes 444 --seed 1 > fixtures/generated/stress-500.fountain    # 501 pages (oracle)
```

Page counts are afterwriting's; Apollo's may differ by a few pages because its split rules differ. The outputs are gitignored, since they can be regenerated.

## Your own scripts

Your existing Fountain scripts are the best real-world import test. Copy them into `reference/own/`, which is gitignored and stays local. A professionally exported PDF of any of them would add a second page-break reference.

## Bounded editable import — `import/writer-subset*`

The original Fountain and hand-written source/record expectations exercise a
representative, explicitly gated writer subset. The checks cover content and
historical imported associations through actual editing, undo, autosave,
reopen, recovery and Save a Copy, plus executable crash/restart flows. These do
not establish full grammar. See [import/README.md](import/README.md) and the
[writer contract](../docs/editable-fountain-subset.md).

## Canonical Fountain export — `export/`

Hand-written expected clean Fountain for the writer-subset model, before and
after the flow test's real edits, with and without the title page. Apollo's
exports must equal these bytes, and aw-parser 0.1.1, the parser inside the
pinned afterwriting, must read them as the hand-worked semantic models. Install
it once with `tools/afterwriting-oracle.sh`, then reconfigure so ctest includes
`fountain_export_oracle_tests`. See [export/README.md](export/README.md) and the
[export contract](../docs/fountain-export.md).

## Edited layout — `editing/`

Hand-derived `.expected` files, in the layout format above, for writing that the
spike Fountain parser cannot express, so the checks edit a saved project
instead. `break-before-space.expected` is fixture 01 after MARA's dialogue gains
a deliberate break followed by two spaces and a longer sentence;
`tests/editor_pdf_tests.py` requires the editor to lay it out without a
QTextLayout mismatch, to agree with its PDF, and the PDF to match this file.
