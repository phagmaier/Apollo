# Apollo — export profile

Status: **numeric profile v1**, 2026-10-27. The values come from measuring the Academy reference. The two visible choices are confirmed by the owner (see "Owner confirmations"). Items marked "Apollo proposal" are delegated detail for review in the visual prototype. The [product spec](spec.md) is authoritative for owner decisions. The PDF painter and editor/PDF agreement pass for the exercised inputs. Native interaction/performance evidence and the remaining limitations are recorded in [spike progress](spike-progress.md).

## Confirmed target

- American feature submission/spec scripts.
- US Letter pages and 12-point Courier.
- Formatted editor pagination matches PDF pagination for the same document/settings.
- Structured title-page fields, with optional title-page inclusion on export.
- App notes and stash excluded from both screenplay export formats.
- English initially; multilingual and emoji features are out of scope.
- Production pagination locks, inserted page numbers, revision colors, and scene-number management deferred.
- Required editor/import/export coverage includes dual dialogue, bold/italic/underline, centered text, lyrics, and manual page breaks, alongside scene headings, action, cues, dialogue, parentheticals, and transitions.
- Convert imported Fountain inline notes to app notes and omitted passages to stash entries; preserve their text in the project and exclude them from clean screenplay exports. Dedicated television templates are deferred.

## How the reference was measured

The reference is the Academy's [screenwriting resources](https://www.oscars.org/nicholl/screenwriting-resources) and its [format guide PDF](https://www.oscars.org/sites/oscars/files/scriptsample.pdf). The guide was last revised in February 2014 and is 13 pages long: a title page, 11 body pages, and a notes page. It was written in Final Draft (embedded font `CourierFinalDraft`). It says scripts should "at least resemble these pages" and that "there is no absolute standard format".

Measurements came from the PDF's text coordinates (poppler `pdftotext -bbox`) and its content streams. One fact limits what the PDF can show. **The PDF was printed through a Windows PostScript driver (`PScript5.dll`), which distorted absolute geometry**:

- Every page is shifted about 0.24 in left. The shift was confirmed by two checks: the title-page lines center on 4.01 in instead of the page center at 4.25 in, and body centered text centers on 4.26 in instead of the text-area center at 4.50 in.
- Glyphs are compressed by a character-spacing operator (`Tc ≈ −0.016`), so characters sit 7.0 pt apart instead of Courier's 7.2 pt (the embedded font's advance is 1228/2048 em, i.e. 10 pitch).

These distortions do not affect what the profile needs:

- distances between element columns (the driver positions each text run exactly);
- the 12-point line pitch;
- where Final Draft broke lines and pages.

Those three are measured facts. Absolute positions below are the measured positions plus the 0.24 in correction.

`tools/academy_reference.py` reproduces the measurement. It writes every body line Final Draft produced (`reference/academy/expected.tsv`) and a Fountain transcription.

Sample page references below include the title page unless explicitly labelled
as body pages. The TSV and `tools/compare_academy.py` number only body pages
1–11, so their page numbers are one lower than the PDF's.

## Line wrapping (measured, exact)

Each wrapped line in the 11 body pages was checked against the next word that did not fit:

| Element | Max characters per line | Evidence |
| --- | --- | --- |
| Action and scene headings | **61** | Longest line 61. Every wrapped line plus its next word is at least 62. |
| Dialogue | **35** | Longest 35; every wrap would have needed 36 or more. |
| Parenthetical | **17 columns, counted from the `(` column** | The `(` hangs one column left of the text; continuation lines start one column in and reach column 17 at most. |

An independent formatter, afterwriting, set to these widths reproduces **345 of the 349** distinct body lines. The 4 lines it misses all belong to the one speech that splits across a page, where its split rule differs (see `fixtures/README.md`). Wrapping breaks at spaces only; a word longer than its column breaks at the limit (no Academy example). Spaces at a line end are not text, so a trailing space never moves a right-aligned or centered line. Emphasis markup is hidden and never counts toward width.

## Proposed geometry (Academy sample pages, corrected)

Inches from the left page edge unless noted. "Measured" values have about ±0.02 in uncertainty from the driver correction.

| Element | Position | Width | Source |
| --- | --- | --- | --- |
| Scene heading | Left 1.5 | 61 columns | Measured (the guide's prose also says left margin 1.5). Uppercase. Not bold: the sample embeds no bold face. |
| Action | Left 1.5 | 61 columns (to 7.6) | Measured. The prose says right margin 1.0 in; 61 columns at true 10 pitch end at 7.6 in, a 0.9 in margin. |
| Character cue | Left 4.0 | to 7.6 (36 columns; a longer cue wraps, and its lines count toward fit) | Measured; the prose says "about 4.0 to 4.2". Uppercase. The sample has no cue long enough to wrap. |
| Parenthetical | `(` at 3.4, text at 3.5 | 17 columns from `(` | Measured; the prose says text starts 0.5 in left of the cue. |
| Dialogue | Left 3.0 | 35 columns (to 6.5) | Measured. The prose says 2.5 in; the owner chose the sample pages (confirmation 1). |
| Transition | Right-aligned to 7.1 | max 56 columns | Measured (`INTERCUT WITH:`, `FADE OUT.`). |
| Centered text | Centered on 4.5 | max 60 columns | Measured (`THE END`, a centered date). |
| (MORE) | Left 4.0 (cue column) | — | Measured. |
| Lyrics | Dialogue column, italic | 35 columns | Apollo proposal; the guide has no lyrics. |
| Dual dialogue | Left column at 1.5, right column at 4.6. Each column: dialogue 28 columns, cue at column + 1.0 in (20 columns: the right cue's room to 7.6 in, also used on the left), `(` at column + 0.4 in (17 columns) | — | Apollo proposal; the guide has no dual dialogue. Check it in the visual prototype. |

Glyphs use Courier Prime's natural advance (0.5996 em) from each line's start position, so a 61-character line is about 0.03 in shorter than exact 10 pitch. Right-aligned and centered positions use the same advance. The editor and the PDF follow the same rule.

## Vertical layout

| Item | Value | Source |
| --- | --- | --- |
| Line pitch | 12 pt (6 lines per inch), set explicitly; Courier Prime's own height is 1.12 em | Measured: every body line is 12.0 pt apart |
| Body slots per page | **56** (top of slot 1 at 1.0 in; slot 56 ends at 10.33 in, a 0.67 in bottom margin) | Measured: page 7 uses slot 56. The prose says top 1.0 in, bottom 0.5 to 1.5 in |
| Baseline | 9 pt below the top of its slot | Apollo proposal; shared by the editor and the PDF |
| Page number | `N.` right-aligned to 7.25 in, line top at 0.5 in; body page 1 shows no number | Measured: right edge 7.25 in (the prose says 0.75 in from the right edge, i.e. 7.75; the owner chose the sample), 0.5 in above the body. The prose permits omitting page 1's number, and the sample omits it |
| Title page | Not counted in page numbers | Sample: the first body page is page 1 |

Blank lines before each element, dropped at the top of a page (measured unless marked):

| Element | Blank lines before |
| --- | --- |
| Scene heading | 2 (also after `FADE IN:` and transitions) |
| Action, cue, transition, centered text, lyrics, dual block | 1 |
| Parenthetical or dialogue within a speech | 0 |

Exception: centered text immediately after a transition has **2** blank lines
before it. The Academy ending has `FADE OUT.` at body slot 44 and `THE END` at
slot 47. This corrects the earlier table's overgeneralization; centered text
after other elements still uses 1 blank line. All leading spacing drops at the
top of a page.

The guide also shows extra blank lines a writer added after a centered line. Whether Apollo preserves author-added vertical space is an editor-behavior decision for later; v1 spacing is automatic.

### Editor states (provisional spike rules)

The editor passes through states a finished script rarely has. So that the
editor and the PDF always paginate them the same way, the engine lays them out
as follows. These are spike rules for owner review, not confirmed behavior:

- **An empty element** (no text, or only spaces) keeps one blank slot with its element's normal spacing,
  so the caret has a line of its own. In a PDF the slot is blank.
- **Spaces at the start of an authored line**, such as after Enter before a
  space in dialogue, are kept in the writing but are not shown and do not count
  toward width: the line starts at its column, as wrapping never begins a line
  with a space. In the editor the caret can still reach them, just left of the
  column. Meaningful indentation and spacing are not supported yet.
- **A cue without speech** stands alone: one or more cue lines with a cue's
  spacing, moved whole with a preceding heading; a cue longer than one page
  is rejected like other unsplittable elements.
- **A right dual cue without a left speech, or without its own speech,** is an
  ordinary cue; the pair is not dual.
- **Parentheticals or dialogue without a cue** keep the speech spacing (one
  blank line before the run, none inside it) and move whole.
- **A typed `(CONT'D)`** on a cue where G1 generates one is replaced by the
  generated form, so it is never doubled. The typed text stays in the document.

## Page-break rules

| Rule | Source |
| --- | --- |
| **B1** A page holds 56 slots. (MORE) and generated cues occupy slots. | Measured |
| **B2** A scene heading never ends a page. It moves with the next element, which then follows its own rules; if that element cannot place at least one line, both move. | Prose: "Scene headers remain attached to description." |
| **B3** A speech (cue, parentheticals, dialogue) that does not fit splits at the **latest sentence end** that leaves the cue plus at least **2 dialogue lines** before the break, with at least 2 lines after it. The page ends with `(MORE)` at the cue column. The next page starts with a generated cue `NAME (CONT’D)`, keeping any extension (`NAME (V.O.) (CONT’D)`), followed by the remaining text **rewrapped from a new line**. If no split qualifies, the whole speech moves. | Prose: "A single line of dialogue is pushed to the following page", plus the MORE/CONT'D description. Observed on pages 4–5, where the split falls mid-line at a sentence end and the rest rewraps. |
| **B4** A parenthetical never ends a page and never directly precedes `(MORE)`. | Consistent with pages 2–3 and 4–5, which B3 alone also explains; otherwise an Apollo proposal |
| **B5** An action paragraph that does not fit splits at the latest sentence end that leaves at least 2 lines on each page, rewrapping the rest; otherwise it moves whole. | Apollo proposal; no action split occurs in the sample |
| **B6** A transition never starts a page. If it does not fit, the preceding element moves with it, re-applying that element's own split rule. | Apollo proposal |
| **B7** Dual dialogue, centered text, and lyric groups never split; they move whole. | Apollo proposal |
| **B8** A manual page break ends the page; the next element starts at slot 1. | Fountain `===` |
| **B9** A sentence end is a word ending in `.`, `?`, or `!` (closing quotes or brackets allowed after it), followed by a space and further text. | Apollo definition, consistent with the observed split |

**Generated `(CONT’D)` after interruption (G1):** when the same character speaks again in the same scene, and only non-speech elements came between, the new cue gets `(CONT’D)` after any extension. The guide calls this "the standard" and notes that many writers drop it. It is on by default (confirmation 2). Generated text is never document text. Fountain export omits it. An imported literal `(CONT'D)` that G1 would generate anyway becomes the generated form, so it is never doubled. The exact import rule belongs to the Fountain mapping work.

### Fit against the Academy pages

Applied to the sample's 10 page breaks, rules B1–B4 reproduce 8. Two are Final Draft behaviors Apollo does not imitate:

- **Page 6 → 7:** Final Draft moved a 3-line speech that fits exactly in slots 54–56. On page 7 it allowed the same pattern. The cause is unknown; it is likely Final Draft's internal metrics.
- **Page 8 → 9:** the one-word line `APRIL` is Final Draft's "Shot" element, which keeps with the next line. Fountain and Apollo have no shot element, so as action it fits in slot 56.

Two smaller differences:

- On a continuation page, Final Draft draws the `(CONT’D)` cue in the top margin above slot 1. Apollo puts it in slot 1.
- The guide's extra blank lines after its centered date have no Apollo equivalent yet (see Vertical layout).

## Title page (proposal from the sample)

Slots are on the body grid: slot n begins at 1.0 in + (n − 1)/6 in.

| Field | Placement | Sample measurement |
| --- | --- | --- |
| Title | Centered on 4.25 (page center), slot 4, shown uppercase | Top ≈ 1.57 in, uppercase |
| Writing credit (for example `Written by`) | Centered, slot 10 | ≈ 2.57 in |
| Author(s) | Centered, slot 15, one line each | ≈ 3.32 in |
| Source / based-on credit | Centered, 2 slots below the last author | Not in sample |
| Contact | Left edge 5.2 in, single-spaced, last line on slot 48 (ends 9.0 in) | Left 5.17 in, 1.5-spaced, last line ends ≈ 9.0 in |
| Draft date | Left edge 1.5 in, on the contact block's last slot | Not in sample |

The sample's title page is not on the body grid, and its vertical correction is about ±0.05 in uncertain. This placement follows it closely but snaps to the grid. The guide's competition advice (remove identifying details for Nicholl entries) is the writer's choice; Apollo's optional title-page export covers it.

## Typeface

[Courier Prime](https://github.com/quoteunquoteapps/CourierPrime) (SIL OFL-1.1), Regular, Bold, Italic, and Bold Italic, pinned and bundled by file hash. All four styles share one advance width (1228/2048 em) and have no kerning, so emphasis never changes layout. Underline position and thickness come from the font's `post` table. Courier Prime covers about 415 glyphs; characters outside it follow the unexpected-character policy that is still open. The guide warns against Courier New, which is about 10% longer.

## Owner confirmations

Answered 2026-10-27:

1. **Where the Academy's pages and prose disagree, follow the sample pages.** That means dialogue at 3.0 in, the parenthetical at 3.4/3.5 in, the cue at 4.0 in, and the page number right-aligned at 7.25 in. The prose alternative was dialogue at 2.5 in and the page number at 7.75 in.
2. **Automatic `(CONT’D)` after action (G1) is on by default.** Page-continuation `(CONT’D)` is always on.

Everything marked "Apollo proposal" above is delegated detail. Review it in the visual prototype rather than now.

## Remaining work before or during the spike

- Fountain serialization of the required elements, literal punctuation, and emphasis escaping, plus the unsupported-construct policy and the CONT'D import rule. The construct inventory is in `fixtures/fountain-syntax/`.
- The unexpected-character policy for glyphs Courier Prime lacks.
- PDF metadata and export failure behavior.

## Verification resources

See [fixtures/README.md](../fixtures/README.md).

- Eight hand-derived layout fixtures cover wrapping, emphasis width, speech split and push, parenthetical protection, heading keep-with-next, action split, the title page, manual breaks, numbering, generated CONT'D, and dual dialogue.
- The local Academy reference holds Final Draft's actual lines for all 11 body pages.
- A deterministic generator produces the 200- and 500-page workloads.
- `tools/pdf_lines.py` dumps any PDF's lines by page and slot for comparison.
- `tools/afterwriting-oracle.sh` is an independent formatter set to these widths.

Apollo's PDF must match the hand fixtures exactly. It must also match the Academy lines, except for the deviations listed above. A second export from Apollo's own engine is not an independent check.
