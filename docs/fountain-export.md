# Canonical Fountain export (`fountain-canonical-1`)

2026-10-28. This is task **2.6d**, a bounded first step of the slice 3/5
Fountain export gate. It exports clean Fountain for **editable projects** —
ordinary Apollo projects and 2.6c editable import copies — when the **current
writing snapshot** stays within the independently verified
[writer subset](editable-fountain-subset.md). It is not complete Fountain
compatibility, and it does not enable PDF export of imported writing. The
contract, spelling choices and bounds are engineering choices within the
delegated compatibility work; the owner-confirmed requirements are those in the
[build plan](writing-build-plan.md#import-and-export-compatibility-gate).

## Eligibility: checked on every snapshot

Eligibility belongs to the writing captured by each export request, not to the
project or its import. Edits made after import can make a project ineligible and
later eligible again; saving is never restricted by it. Every element in the
snapshot (title fields only when the title page is included) must satisfy:

- **The writer-subset content rules**, shared in code with import admission
  (`writer_subset_gaps`): printable ASCII and é only; scene headings, character
  cues and the title in ASCII capitals; line breaks only in action and dialogue;
  title fields of at most 48 characters; no empty element/line and no leading,
  trailing or repeated spaces. A page break carries no text. The generated
  `(MORE)` type is refused.
- **Speech structure:** a character cue is followed by parentheticals and
  dialogue with at least one dialogue element; dialogue/parentheticals never
  stand outside a speech; two adjacent dialogue elements would merge, so they are
  refused. A dual cue must directly follow one speech whose cue is not itself
  dual. A parenthetical begins with `(` and ends with `)` outside any emphasis.
- **Unambiguous spelling** (below). Text that standard Fountain would read as
  something else — a `#` scene number in a heading, a capital line ending in
  `TO:`, an inner line starting with `# = > @ ~ !`, `.`+letter/digit or
  `I./E.`, a parenthesized dialogue line, a cue ending in `^`, a transition
  ending in `<`, note/boneyard markers `[[ ]] /* */`, or a cue name with no
  letter — is refused.
- **Emphasis that Fountain can spell:** per line, never including a line break,
  not starting/ending with a space, not beginning inside a word, and properly
  nested; bold/italic runs may not touch or share a boundary (underline may).
- **Size:** at most 1 MiB of output, the bound of the verifying reader.

Each refusal names the element by its position in saved order (title page
first), its type and the start of its text, with the reason, e.g.
`Element 12, Character cue “June”: is not written in capital ASCII letters, …`.
The export panel lists up to eight reasons and the count of the rest. The
request is refused before any file is touched.

## Canonical spelling

Elements are separated by one blank line; a speech's lines are consecutive.
Output is UTF-8 with LF, no BOM, and a final newline. Fountain whitespace and
marker spelling are not byte-identical to an imported original.

| Element | Spelling |
| --- | --- |
| Title page | `Title:`, `Credit:`, `Author:`, `Source:`, `Contact:`, `Draft date:` in saved order; a run of one field uses `Key:` and three-space continuation lines; a blank line follows |
| Scene heading | Unforced with a standard prefix (`INT`, `EXT`, `EST`, `INT./EXT`, `INT/EXT`, `I/E`, then `.` or space), otherwise `.`-forced when it starts with a letter/digit |
| Action | Unforced when the first line starts with a letter/digit, has lowercase before any `(`, and is neither heading-like nor an opening `Key:`; otherwise `!`-forced. Authored breaks are lines |
| Character cue | As written; `@`-forced when it starts with punctuation or would open a title page; ` ^` marks the second speech of a dual pair |
| Parenthetical / dialogue | Lines directly after the cue; authored breaks are lines |
| Lyric | `~text` |
| Transition | `>text` |
| Centered | `> text <` |
| Page break | `===` |
| Emphasis | `_underline_`, `*italic*`, `**bold**`, `***bold italic***`, nested outer-first; underline outside an equal run; literal `\*`, `\_`, and `\\` only where a backslash would escape the next character |

Without **Include title page**, title fields are neither checked nor written.

## Verification of each export

After the explicit checks pass, the worker reads the produced text with the
frozen revision-1 interpreter (`interpret_fountain`). It must report no
unresolved syntax, no retained notes/stash/organization records, no layout
limitation other than authored case/breaks, and exactly the snapshot's types,
text, emphasis spans, dual flags and title fields. Otherwise the file is not
written and the located verification reason is shown. This guard is a runtime
safety net, not the correctness oracle: tests compare with hand-written text and
an independent parser. A deterministic 40,000-snapshot property test checks that
the explicit checks explain every refusal; exploratory runs over 2.4 million
random snapshots found and fixed four unexplained cases (a style on a line break
alone, an opening `Key:` cue or heading, and `.é` on an inner line).

## Omitted material and disclosure

Clean output never contains imported notes, stash, sections, synopses,
production scene numbers, unsupported title-page fields or whitespace-only
source lines. They stay in the project's immutable imported material and
original. For editable import copies the panel, completion status and CLI state
the counts from the frozen interpretation, for example: `Not included in clean
Fountain: 1 imported note, 1 stash passage, 1 unsupported title-page field, 1
section, 2 synopses, 1 production scene number.` A title page left out by choice
is also stated. Ordinary projects have no retained imported material.

## Snapshot, files and departure

- The request captures the document on the GUI thread and runs the linear
  checks there; verification, serialization and replacement run on a worker.
  Writing, autosave and undo continue. The status names the capture time, e.g.
  `Fountain snapshot from 17:36:37 exported to …`, without implying later edits.
- The destination is replaced through `QSaveFile`: a refusal, blocked snapshot,
  failed write or interrupted worker leaves any existing file unchanged.
- Destinations inside the active project folder (updated after Save a Copy) and
  the external file an import came from are refused. Review-only intakes cannot
  export; use **Edit in a New Copy** first.
- Leaving for Home or closing waits for an accepted export, as for PDF.

## Using it

- Writing desk: **Export** → **Fountain** (or Ctrl+Shift+E), choose the
  destination and title-page option, read the eligibility/disclosure, then
  **Export Fountain**. PDF remains in the same panel; for imported writing it
  shows the separate PDF block.
- Command line, from the saved writing (no lock; a live writer's unsaved edits
  are not included): `build/release/apollo_editor PROJECT --export-fountain
  OUT.fountain [--no-title-page]`. Exit 0 exported, 3 blocked/refused, 1 failed.

## Evidence

Independent expectations are in [fixtures/export](../fixtures/export/README.md).

| Check | What it establishes |
| --- | --- |
| `fountain_export_tests` | Hand-written canonical text for the whole writer-subset model, with and without title page; 23 hand-written spelling cases (forcing, prefixes, cues, dual, nested emphasis, escapes, multi-value title fields); 30 refusal cases with located reasons; per-snapshot eligibility changes; omission counts; size bound; explanation coverage |
| `project_access_flow_tests` | Real QML controls: import → Edit in a New Copy → panel disclosure → export equal to hand-written text; real-key edits that block (leading space after a break, empty element, mixed-case cue) and recover; export while the worker is held and typing continues, autosaved later words excluded; PDF still blocked; reopen and export without title page; failed write and refused destinations keep files; ordinary project export and Home waiting for it |
| `fountain_export_process_tests` | Executables: `--edit-fountain`, real typing with autosave that blocks then permits export in a new process, exact bytes, disclosure, no-title body, refusals of project/original/review-only |
| `fountain_export_oracle_tests` | aw-parser 0.1.1 (afterwriting's MIT parser, via the pinned afterwriting 1.17.3) reads both process exports and all three hand-written expected files as the hand-worked semantic models: types, text, decoded emphasis, dual pairing, title fields and breaks. Its one representation difference is explicit: it reads a lyric as italic action. Mutating a dual marker or emphasis makes the comparison fail |

The oracle test is configured only when aw-parser and node are present; run
`tools/afterwriting-oracle.sh` once (it needs network, node and pnpm), then
reconfigure. `node tools/aw_tokens.cjs AW_PARSER_DIR FILE` prints its reading.

Measured on this machine (release build, synthetic repeats of the subset model):
2,106 elements / 40 KB take 4 ms for the GUI-thread check and 11 ms for the
worker; 10,506 elements / 200 KB take 20 ms and 57 ms. Realistic full-length
scripts mostly exceed the subset today (for example mixed-case cues), so this
is not a representative-workload measurement.

## Remaining gates

- Mixed-case headings/cues/titles, other Unicode, meaningful spacing, empty
  elements, multiline title/lyric/centered text, lyric within speech, adjacent
  dialogue elements and complex emphasis remain refused, not exported lossily.
  Typing headings in lowercase, which the editor shows uppercase, is refused.
- Complete Fountain grammar and corrections remain 2.6b work. aw-parser was
  checked on these fixtures only; it differs from Fountain 1.1 where it reads
  inner action lines as headings/transitions or `Key:` lines anywhere before the
  first heading, so a general claim of agreement with other applications is not
  made.
- PDF export of imported writing stays blocked pending independent layout
  evidence. The editor/engine layout warning seen when a dialogue line starts
  with a space after a deliberate break is outside this task.
- Not verified: power-loss durability of the replaced file, very large scripts
  near 1 MiB in the UI, a watchdog/cancel for a stuck worker, and owner comfort.
