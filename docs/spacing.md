# Spacing and deliberate breaks

2026-10-28, task **3b**: a bounded step of slice 3's spacing contract. It defines
how authored spaces, deliberate breaks and intentional blank dialogue lines
behave in writing, layout, PDF, clean Fountain export and Fountain import
admission. It does not add creation controls or advisories for unusual
spacing, and it does not enable PDF export of imported writing.

## Confirmed requirements

From the [spec](spec.md#writing-and-formatting) and [planning](planning.md)
rounds 20–21:

- *Apply element spacing and indentation automatically… Do not silently rewrite
  dialogue or action.*
- Enter midway through dialogue continues the same speech on the next line.
  *Preserve that deliberate break through editing, persistence, Fountain
  interchange, and PDF layout; do not flag it as invalid.*
- *Intentional blank spacing within dialogue remains possible and is not
  silently removed; its creation controls remain to be specified.*
- The [compatibility gate](writing-build-plan.md#import-and-export-compatibility-gate):
  manual line breaks, whitespace-only dialogue lines and intentional action
  spacing/indentation must be distinguished from wrapping and element
  separators, and preserved or explicitly surfaced before an affected export.

## Evidence used for the choices

- **Fountain syntax** ([official samples](../fixtures/fountain-syntax/)). Action
  keeps its spaces, so indentation there has printed meaning (`Indenting`).
  Leading whitespace before cues, parentheticals and dialogue is only cosmetic
  in the same sample. A line of two spaces inside dialogue is a blank line of the
  speech; a truly empty line ends it (`Dialogue`). The same samples contain a
  trailing space in dialogue and two spaces after a period in action.
- **aw-parser 0.1.1**, the independent oracle, read a probe file this way. Dialogue
  lines lose their leading and trailing spaces, a two-space line inside
  dialogue is an empty dialogue line, and repeated spaces inside action and
  dialogue lines are kept. Action is kept as written, including leading spaces.
- **Apollo's revision-1 interpreter** drops leading spaces of dialogue lines,
  keeps trailing ones and keeps a line of spaces inside dialogue as written. A
  line of spaces ends an action paragraph.
- **The engine** (`engine/wrap.cpp`) never begins or ends a shown line with a
  space, for authored and wrapped lines alike, and gives every authored line,
  including an empty one, at least one slot.
- **Defects found** while probing. A speech split at the space in `thirteen.
  \nFourteen` started the continued page with a printed blank line, and the
  editor reported `QTextLayout did not break block N where the engine did`.
  Without the space (`thirteen.\nFourteen`), the same sentence end was not a
  split point at all, so the whole speech moved and left the page short. An
  invisible space changed pagination.

## Authored spacing and presented spacing

As in the [case contract](case-handling.md), the project keeps **authored text**
exactly as typed, pasted or imported. Spacing is **presented** as follows, in the
editor, the PDF and clean Fountain alike.

| Authored | Shown and printed | Clean Fountain (`fountain-canonical-3`) | New import copies (`writer-subset-3`) |
| --- | --- | --- | --- |
| Spaces at the start or end of an authored line, in any element except title fields | Not shown and not counted toward width: the line starts at its element's column. The editor's caret can reach them just outside the text | Not written; the export discloses on how many lines | Dialogue: admitted. Leading spaces of dialogue lines are not retained, as Fountain ignores them; trailing ones are. Action: review-only, because Fountain prints its indentation. Other types: review-only |
| Repeated spaces inside a line | Shown and counted, as typed; at a wrap point they are a break point and are not shown | Action and dialogue: as written. Other types: refused | Action and dialogue: admitted. Other types: review-only |
| An empty authored line, or one of only spaces, inside dialogue | One blank slot inside the speech at the dialogue column, with no added spacing | A line of two spaces | A line of spaces inside dialogue: admitted, kept as written |
| A blank first or last line of dialogue | A blank slot | Refused: *begins or ends with a blank line, which Fountain cannot keep in the speech* | Review-only |
| A blank line inside action | A blank slot | Refused: *has a blank line, which Fountain reads as the end of the action* | Not produced: a line of spaces ends the action |
| Spaces in title fields | Shown as written, centered with them (the title page is not wrapped) | Refused, as before | Review-only |
| An element of only spaces | One blank slot | Refused as empty | Review-only |

Tabs remain outside the subset everywhere: plain paste turns them into spaces,
import keeps them review-only, and the export refuses them.
This plain-paste rule describes the screenplay body editor; the
[structured title form](source-format.md#structured-title-page-form-slice-3-task-3d)
uses plain text fields without the body's paragraph/type conversion.

### Writing

- Enter inside dialogue inserts a deliberate break (unchanged). Pressing it
  after a space leaves the space before the break; pressing it before a space
  leaves the space at the start of the new line. Neither is shown.
- Enter at the start of a dialogue line, such as a second Enter after a break,
  adds a blank line to the same speech. Each Enter is one undo step. This is the
  existing keyboard behavior, not a new creation control. The owner has not yet
  specified creation controls or an advisory for unusual blank spacing.
- Nothing in the writing is rewritten. Saving, autosave, recovery, reopen,
  copying and undo keep authored spaces and empty lines exactly.
- **Ctrl+Enter** inserts a non-text [manual page-break marker](source-format.md#manual-page-break-authoring-slice-3),
  not a deliberate line break or blank dialogue line. It keeps the whole speech
  together; ordinary Enter retains the dialogue behavior above.

### Page splits

This refines profile rules B3, B5 and B9 ([export profile](export-profile.md)):

- A sentence end followed by a deliberate break is a split point, like one
  followed by a space. Spaces around the break are not shown, so the continued
  page resumes at the next authored line. `turned.\nNow`, `turned. \nNow` and
  `turned.\n  Now` paginate identically.
- A split never puts an authored blank line, or a blank speech part such as an
  empty parenthetical, directly before `(MORE)` or the end of a page, or
  directly after the continued cue or the top of the next page. The speech or
  paragraph splits at an earlier sentence end, or moves whole.

The Academy reference has no deliberate breaks, so these rules are Apollo
proposals checked against hand-derived examples only.

## Engineering choices for owner review

These follow from the confirmed rules and evidence above. They were not decided
separately:

1. **Leading spaces are not displayed and do not count toward width.**
   Indentation is automatic. Fountain gives dialogue indentation no meaning, and
   Apollo has no indentation feature for action. The spaces stay in the writing,
   and the caret sits on them just left of the column, which may look unusual.
2. **Clean Fountain writes the presented spacing and discloses it.** This
   follows the accepted case policy: the file reads as Apollo shows and prints
   it, and the project keeps what was typed. The alternative, writing authored
   spaces, would make other applications indent action that Apollo does not
   indent. Trailing spaces are invisible in every output, but they are counted
   and disclosed too, so the export never silently differs from the writing.
3. **Blank dialogue lines can be written only inside a speech.** Fountain readers
   disagree about a line of spaces right after a cue (aw-parser reads
   `THEO\n  \n` as a shot), and a blank last line is indistinguishable from the
   speech's end in most renderers.
4. **Import admits only spacing whose printed meaning both readers agree on.**
   Action indentation and tabs stay review-only. The later bounded scene-gap
   choice below is an explicit normalization, not an exception to that fidelity claim.

## Contract revisions

- **`fountain-canonical-3`** replaces `-2`'s refusal of every leading, trailing or
  repeated space and empty line with the presentation table above.
- **`writer-subset-3`** applies to new import copies. Copies created under
  `writer-subset-1` or `-2` keep being validated by their own stricter spacing
  rules, and builds before task 3b refuse `writer-subset-3` copies instead of
  misreading them.

## Evidence

| Check | Independent expectation |
| --- | --- |
| `layout_tests` deliberate-break splits | Hand-worked pages: three spellings of a break after `thirteen.` split identically and the continued page starts at `Fourteen`, mapped to its first visible byte. A blank line or an empty parenthetical beside a candidate split moves the split earlier or the speech whole. An action paragraph splits after `Line three. \n` and keeps a blank line inside a page. All eight fail on the previous engine |
| `editor_tests` `deliberate_break_splits_speech_at_page_end` | A real Enter on either side of the space in fixture 02's `turned. Now` gives exactly the hand-derived `02-dialogue-split.expected` pages, with the caret on page 2 slot 2 and no stray ink; undo restores the writing and pages. On the previous engine one row raised the QTextLayout error and the other moved the speech |
| `editor_tests` `enter_twice_makes_blank_dialogue_line` | Two real Enters in fixture 01: a blank slot 10 at the dialogue column, later elements one slot lower, typing fills the line, and undo/redo step through each state |
| `editor_pdf_tests` `blank-dialogue-line`, `break-at-page-split` | Saved projects with a space before a break, a line of spaces, trailing spaces and a break at a page split: `--check`, editor/PDF agreement, and PDFs equal to `fixtures/editing/blank-dialogue-line.expected` and `break-at-page-split.expected` |
| `fountain_export_tests` | Hand-written spellings for line-edge spaces in every body element type, blank dialogue lines (two spaces), lines of spaces, repeated spaces and emphasis after stripped spaces. Located refusals for blank action lines, blank first/last dialogue lines, repeated spaces in a heading, spaces in a title and spaces-only elements. Disclosure counts; the spacing model exports to hand-written `fixtures/export/spacing-subset.fountain`; the explanation property now generates repeated spaces and blank lines. A further exploratory run of 1,000,000 spacing-weighted snapshots found no refusal left to the read-back guard alone |
| `fountain_conversion_tests` | `fixtures/import/spacing-subset.fountain` converts to the hand-written authored model; `writer-subset-3` admits it with exact original bytes; a `writer-subset-2` claim and legacy checks refuse its spacing. Indented or trailing-space action, repeated spaces in a heading or cue, and blank first or last dialogue lines stay review-only |
| `project_access_flow_tests` `typed_spacing_is_kept_and_exported_as_presented` | Real keys in the Writing desk: Enter after a space and again; undo/redo; autosave and reopen keep `At the \n\nsame time.`; the panel discloses one line; the export through real controls, and after reopening, equals hand-written `At the\n  \nsame time.`. A speech made to begin with a blank line is refused with its reason and nothing is written; Backspace corrects it. The 2.6d flow's Enter before a space is now eligible with the disclosure |
| `fountain_export_process_tests` / `fountain_export_oracle_tests` | Executables: typing ending in a space exports without it, with the disclosure, while the saved writing keeps it. The spacing fixture is imported, typed into and exported as hand-written text; authored source and original bytes are unchanged; indented action is refused. aw-parser 0.1.1 reads the export, the hand-written export and the original fixture as the presented model |

### Native owner check

Run `tools/check-spacing.sh`. It builds release and opens an ordinary synthetic
project, with isolated XDG application state and all work/logs retained in a new
ignored `build/spacing-check-*` directory. The printed checkpoints cover Enter
before an existing dialogue space, a second Enter for an internal blank line,
Left/Right across the hidden leading space, and Ctrl+Shift+E's spacing disclosure
with an export outside the project. No private source or owner project is used.
`tools/check-spacing.sh --smoke` automates the existing focused native spacing,
persistence/disclosure and export checks without recording owner acceptance.
The choices above remain provisional pending the owner's feel assessment.
`fountain-canonical-4` / `writer-subset-4` inherit revision 3's spacing unchanged.

## Explicit scene-spacing choice (`writer-subset-5`)

The full-length assessment's 49 vertical-spacing locations are **49 separate
runs of exactly two empty lines before scene headings**: 37 after action,
12 after dialogue. They are not internal dialogue blanks, and do not overlap
the two unfinished cue/parenthetical-only speeches.
[Fountain's syntax](https://fountain.io/syntax/) promises vertical
whitespace preservation; aw-parser with `merge_multiple_empty_lines: false`
retains an extra separator token. Apollo therefore must not call collapsing
these runs equivalent output.

The smallest implemented workflow is an **unchecked-by-default import choice**:
**Use Apollo's normal scene spacing**. It covers only the exact pattern above,
with no notes/stash/other material inside the gap. It retains the original
bytes and frozen located limitations, surfaces each omitted extra line in
the working imported-material reader, and discloses the count and
non-equivalence in the Fountain panel, completion status and CLI. The writer
uses normal profile spacing (two slots before headings, dropped at page tops);
clean Fountain uses one separator. No invented blank action/dialogue element
or new source-schema field is introduced.

Before publication, clear the choice or cancel. After publication, the old
review and complete original remain available to start another copy; the
original spacing is never rewritten. New metadata v2 records the choice.
Other vertical runs, whitespace-only lines and removed material within gaps
stay review-only, even with the option selected. This is intentionally not
editable arbitrary vertical spacing or spacing-equivalent Fountain export.

Empty speeches are a separate structural issue. They remain exact, saveable
unfinished writing with a visible cue list and blocked Fountain output. **Treat
speech as action** explicitly changes only cue/parenthetical types, in one
Undo step; no words, styles or IDs are inserted or removed. Adding dialogue
manually is the alternative. Dual-related automatic correction is refused.
Ordinary deliberate breaks/intentional internal dialogue blanks are not flagged.


## Remaining limitations

- No dedicated control or advisory for blank spacing; creation controls remain
  an owner decision ([spec](spec.md#writing-and-formatting)).
- Action indentation is not supported: typed leading spaces are not shown, and
  indented Fountain action stays review-only.
- Title fields show spaces as written, unlike body text, and refuse them in
  export.
- Blank lines in action, blank first or last dialogue lines, repeated spaces in
  headings, cues, parentheticals, transitions, lyrics and centered text, tabs,
  and extra vertical space between elements remain refused or review-only,
  except for the explicit, non-equivalent scene-spacing choice above.
- Imported writing still cannot be exported as PDF; its layout needs separate
  requirements and evidence.
- Not verified: owner comfort with invisible spaces and the caret position left
  of the column, the disclosure wording, and find/replace, which does not exist
  yet and will need to choose between authored and presented text.
