# Canonical Fountain export (`fountain-canonical-4`)

2026-10-28. This is task **2.6d**, a bounded first step of the slice 3/5
Fountain export gate. It exports clean Fountain for **editable projects** —
ordinary Apollo projects and 2.6c editable import copies — when the **current
writing snapshot** stays within the independently verified
[writer subset](editable-fountain-subset.md). It is not complete Fountain
compatibility, and it does not enable PDF export of imported writing. The
contract, spelling choices and bounds are engineering choices within the
delegated compatibility work; the owner-confirmed requirements are those in the
[build plan](writing-build-plan.md#import-and-export-compatibility-gate).
Revision 2 (task 3a) writes scene headings and character cues as presented, in
capitals, under the [case contract](case-handling.md); revision 1 refused them
unless authored in capitals. Revision 3 (task 3b) writes spacing as presented
under the [spacing contract](spacing.md): no spaces at line edges, a blank
dialogue line as two spaces, and repeated spaces in action and dialogue as
typed. Revision 2 refused every leading, trailing or repeated space.
Revision 4 adds exact Unicode punctuation **— ‘ ’ “ ” …** (U+2014, U+2018,
U+2019, U+201C, U+201D, U+2026). No ASCII substitution or normalization is
performed. Title text still requires ASCII capitals; all other case, spacing,
structure, size and PDF gates remain unchanged.

## Eligibility: checked on every snapshot

Eligibility belongs to the writing captured by each export request, not to the
project or its import. Edits made after import can make a project ineligible and
later eligible again; saving is never restricted by it. Every element in the
snapshot (title fields only when the title page is included) must satisfy:

- **The writer-subset content rules**, shared in code with import admission
  (`writer_subset_gaps`): printable ASCII, é and the six punctuation marks above;
  scene headings and character cues in any ASCII case but with no letter that
  presentation leaves lowercase (é); the title in ASCII capitals; line breaks only in action and dialogue;
  title fields of at most 48 characters. A page break carries no text. The
  generated `(MORE)` type is refused.
- **Spacing** ([spacing contract](spacing.md)): no element whose presented text
  is empty; no blank line in action; no blank first or last dialogue line;
  repeated spaces only in action and dialogue; title fields without leading,
  trailing or repeated spaces.
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
| Scene heading | Presented text (ASCII capitals). Unforced with a standard prefix (`INT`, `EXT`, `EST`, `INT./EXT`, `INT/EXT`, `I/E`, then `.` or space), otherwise `.`-forced when it starts with a letter/digit |
| Action | Unforced when the first line starts with a letter/digit, has lowercase before any `(`, and is neither heading-like nor an opening `Key:`; otherwise `!`-forced. Authored breaks are lines |
| Character cue | Presented text (ASCII capitals, extensions included); `@`-forced when it starts with punctuation or would open a title page; ` ^` marks the second speech of a dual pair |
| Parenthetical / dialogue | Lines directly after the cue; authored breaks are lines; a blank dialogue line is two spaces |
| Lyric | `~text` |
| Transition | `>text` |
| Centered | `> text <` |
| Page break | `===` |
| Emphasis | `_underline_`, `*italic*`, `**bold**`, `***bold italic***`, nested outer-first; underline outside an equal run; literal `\*`, `\_`, and `\\` only where a backslash would escape the next character |

Task 3c's [selection authoring](source-format.md#emphasis-authoring-slice-3-task-3c)
uses these existing emphasis rules without widening them or revising
`fountain-canonical-4` / `writer-subset-5`. A style command may leave a snapshot
outside the spellable subset; it remains saveable and gets the same located
export refusal. Real command-authored exports and fresh-process re-exports of
the original `emphasis-authoring` fixture are checked against hand-written
semantics and canonical bytes by aw-parser, not only Apollo's interpreter.

Without **Include title page**, title fields are neither checked nor written.

The structured title form (task 3d) preserves multiline and blank writing in
source and supports its bounded PDF layout; it does **not** widen this
Fountain contract. A cleared existing entry is still a saved empty field until
explicitly removed, and receives the existing refusal when included. An unused
absent field left empty creates no element. Apply commits all form changes in
one document undo step. A request captures only applied changes, and a later
Apply cannot change its snapshot. The real-controls checks and independent
aw-parser now also verify six authored title fields, an accepted snapshot
followed by a later title edit, body-only output and fresh-process re-export.


Manual page-break authoring uses the existing empty marker and canonical `===`;
it does not widen `fountain-canonical-4` / `writer-subset-5`. The real-controls
`page-break-authoring` case preserves the whole speech and dual pair, saves and
reopens, then exports hand-written canonical bytes. Fresh-process re-export and
bounded reimport use the same gates; imported PDF remains blocked. An empty
action created after a final marker remains unexportable until written or
explicitly removed. Legacy marker text is retained in source and refused by
both clean Fountain and PDF, not exported as a blank marker.

Dual-dialogue authoring uses the existing second-cue ` ^` spelling and keeps
all current snapshot eligibility checks. The real-controls
`dual-dialogue-authoring` fixture adds only that cue's flag, saves/reopens,
exports hand-written bytes, and is read by aw-parser. Poppler independently
checks ordinary-project dual columns and adjacent-break pages; oversized pairs
still refuse PDF and imported PDF stays blocked. See [dual-dialogue.md](dual-dialogue.md).

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

When a heading or cue was authored in lowercase, the same places say that it is
written in capitals and that the project keeps the typed case, for example
`Written in capitals, as Apollo shows and prints them: 1 scene heading and 2
character cues typed in lowercase. The project keeps the case as typed.`
When lines with text had spaces at an edge, they say `Written without the
spaces at the start or end of 2 lines, which Apollo does not show or print. The
project keeps them as typed.`

Imported copies made with the explicit scene-spacing choice disclose, e.g.,
`By explicit import choice, 49 extra empty lines before scene headings use
Apollo's normal scene spacing and are not written.` The disclosure states
that this is **not spacing-equivalent output** and that the original lines and
locations stay in imported material. The unchecked import gate is the default;
export cannot silently opt into normalization. Canonical revision 4's spelling
is unchanged. `writer-subset-5` permits unfinished empty speeches to enter the
writer, but this export contract still refuses them until manually corrected
or explicitly, undoably treated as action.

## Snapshot, files and departure

Task **5b** (2026-11-05) replaces the in-process worker with a bounded helper
lifecycle. The canonical contract, per-snapshot eligibility, independent
aw-parser gate and imported PDF restriction remain unchanged.

- The request captures owned document elements, styles, dual flags and the
  title choice on the GUI thread and runs the explicit eligibility checks there.
  The `fountain_export` helper verifies and serializes this fixed snapshot;
  writing, autosave, assistance and Undo continue separately. Busy and success
  status identify the capture date/time to milliseconds. Success explicitly
  says later edits are not included and retains title/case/spacing/import
  omission disclosures. Only one Fountain request runs at a time.
- The parent creates an owned temporary file on the destination filesystem.
  The helper writes only that staging file. A clean helper exit and valid timing
  response permit the parent to publish through atomic rename, retaining
  existing destination permissions. Failure or cancellation preserves existing
  destination bytes and removes staging after reaping the helper. Cancellation
  wins even after the helper has serialized a complete file but before its exit
  is accepted. The saved-writing CLI uses this same helper and publication path.
- Destinations inside the active project folder (updated after Save a Copy),
  file/parent aliases into it, and the external original an import came from
  are refused. A path within the project remains refused even if its final
  symlink points outside. Review-only intakes cannot export; use **Edit in a
  New Copy** first.
- Reopen **Export** → **Fountain** for **Cancel Export**. Cancellation kills
  asynchronously and stays busy until the helper is reaped; it reports
  cancellation separately from failure. A subsequent request can retry.
- After 30 seconds, **Cancel Export / Keep Waiting** appears. Keep Waiting
  returns focus to writing and starts another 30-second interval without
  replacing the accepted snapshot. Tests shorten the timer through a C++ seam.
- Home drains saves and the accepted export asynchronously; watchdog cancellation
  lets it finish if the helper has stopped. Close offers Cancel Export / Keep
  Waiting immediately. Keep Waiting retains the pending close; completion or
  cancellation retries through autosave, protecting writing entered afterward.
  Save/recovery failure choices remain in force.
- Teardown without an event loop kills unfinished work and waits up to one
  second for process completion, then removes staging without publication.
  Normal departure uses the asynchronous choices above. Kernel-level filesystem
  stalls during capture, staging creation or publication are not established
  as cancellable here; production responsiveness measurements remain task 5c.

`export_job_tests` checks actual helper startup/stops, cancellation after full
serialization, process reaping, bounded destruction, crashes, failed exits,
malformed responses, publication failure, retry, exact prior bytes, permissions
and temporary cleanup. Its fixed-snapshot case destroys the changed live
writing, then compares the export with hand-written canonical text. Real QML
controls check watchdog rearming, typing and explicit cancellation. Project
flows check Home/Close, success after pending Close, newer autosaved writing,
Save a Copy destination protection, and unchanged import/title/case/spacing
exports. Existing process/aw-parser checks exercise the same QML helper path.

Run `QT_IM_MODULE=compose tools/inspect-writing-desk.sh --smoke` for the focused
native Wayland path and saved logs, including both exports' lifecycle/departure
checks. Qt Test supplies their key/mouse events. Full compositor/physical input,
fcitx, sustained responsiveness and owner comfort are separate evidence. See
[the task result](writing-build-plan.md#task-5b--bounded-fountain-export-lifecycle)
for actual verification outcomes and limits.

## Using it

- Writing desk: **Export** → **Fountain** (or Ctrl+Shift+E), choose the
  destination and title-page option, read the eligibility/disclosure, then
  **Export Fountain**. PDF remains in the same panel; for imported writing it
  shows the separate PDF block.
- Command line, from the saved writing (no lock; a live writer's unsaved edits
  are not included): `build/release/apollo_editor PROJECT --export-fountain
  OUT.fountain [--no-title-page]`. Exit 0 exported, 3 blocked/refused, 1 failed.
  With no window to offer Keep Waiting, this command explicitly reports and
  cancels a helper that exceeds 30 seconds, exits 1 and preserves existing output.

## Evidence

Independent expectations are in [fixtures/export](../fixtures/export/README.md).

| Check | What it establishes |
| --- | --- |
| `fountain_export_tests` | Hand-written canonical text for the whole writer-subset model, with and without title page, and of the lowercase case and spacing models; 35 hand-written spelling cases (forcing, prefixes, cues, dual, nested emphasis, escapes, multi-value title fields, lowercase headings and cues, line-edge spaces, blank dialogue lines, repeated spaces); 36 refusal cases with located reasons; case and spacing disclosure counts; per-snapshot eligibility changes; omission counts; size bound; explanation coverage |
| `project_access_flow_tests` | Real QML controls: import → Edit in a New Copy → panel disclosure → export equal to hand-written text; real-key edits: a space left after a break is eligible and disclosed, a speech beginning with a blank line and an empty element block and recover; a separate flow types a space before a break and a blank dialogue line, undoes, redoes, saves, reopens and exports hand-written text with the spacing disclosure; a cue typed in mixed case exported in capitals with the case disclosure; a separate ordinary-project flow types, edits, undoes, saves and reopens lowercase headings/cues and exports hand-written capitals; export while the worker is held and typing continues, autosaved later words excluded; PDF still blocked; reopen and export without title page; failed write and refused destinations keep files; ordinary project export and Home waiting for it |
| `fountain_export_lifecycle_process_tests` | CLI write failures retain exit 1; an isolated stopped helper exercises the production 30-second timeout, explicit cancellation/failure, exact prior bytes, process reaping and staging cleanup |
| `fountain_export_process_tests` | Executables: `--edit-fountain`, real typing with autosave ending in a space that a new process exports without it and discloses, then one more key; a refused ordinary project leaving an existing file intact; exact bytes, disclosure, no-title body, refusals of project/original/review-only; the lowercase case fixture imported, typed into and exported as hand-written capitals with disclosure, original bytes and authored source unchanged, `@McKay` refused; the spacing fixture imported, typed into and exported as hand-written text, authored spaces and original bytes kept, indented action refused |
| `fountain_export_oracle_tests` | aw-parser 0.1.1 (afterwriting's MIT parser, via the pinned afterwriting 1.17.3) reads the process exports and the hand-written expected files, including the case and spacing fixtures and the original spacing fixture itself, as the hand-worked semantic models: types, text, decoded emphasis, dual pairing, title fields and breaks. Its one representation difference is explicit: it reads a lyric as italic action. Mutating a dual marker or emphasis makes the comparison fail |

The oracle test is configured only when aw-parser and node are present; run
`tools/afterwriting-oracle.sh` once (it needs network, node and pnpm), then
reconfigure. `node tools/aw_tokens.cjs AW_PARSER_DIR FILE` prints its reading.
The later `punctuation-subset` fixtures add hand-written semantics and canonical
text for **— ‘ ’ “ ” …**. Conversion tests retain earlier contracts' refusal of
these characters; the QML flow types, undoes/redoes, saves, reopens and exports
an ellipsis while checking exact authored text/styles and the separate PDF
block. The executable/oracle flow reads the original, expected and typed export
with aw-parser, including the bold code-point span after a curly apostrophe.

Historical measurements before task 5b (release build, synthetic repeats of the subset model):
2,106 elements / 40 KB take 4 ms for the GUI-thread check and 11 ms for the
worker; 10,506 elements / 200 KB take 20 ms and 57 ms. These are not
representative-script measurements and do not measure the new helper lifecycle.
Current production measurements remain task 5c. The later
[full-length assessment](fountain-compatibility.md#representative-full-length-assessment-2026-10-28)
records the original assessment and subsequent workflow evidence. The owner
reference's path requires explicit spacing and empty-speech choices; the second
reference remains gated.

## Remaining gates

- Headings/cues with letters not shown in capitals (é), titles not in
  capitals, other Unicode, the spacing listed as refused in
  [spacing.md](spacing.md), empty elements, multiline
  title/lyric/centered text, lyric within speech, adjacent dialogue elements and
  complex emphasis remain refused, not exported lossily. See
  [case-handling.md](case-handling.md) for the remaining case limitations.
- Complete Fountain grammar and general corrections remain 2.6b work.
  aw-parser was checked on these fixtures and the bounded representative
  workflow, not every syntax combination. It differs from Fountain 1.1 where
  it reads inner action lines as headings/transitions or `Key:` lines anywhere
  before the first heading; no general agreement claim with other applications
  follows.
- PDF export of imported writing stays blocked pending independent layout
  evidence. The editor/engine layout warning seen when a dialogue line starts
  with a space after a deliberate break was an editor placement bug, since
  fixed; see the [build plan](writing-build-plan.md#layout-fix--spaces-after-a-deliberate-break).
  Since revision 3 such a line is written without the space, with the disclosure.
- Not verified: power-loss durability of the replaced file, very large scripts
  near 1 MiB in the UI, cancellation of kernel-level filesystem stalls, and owner comfort.
